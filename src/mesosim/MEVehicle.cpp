/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2020 German Aerospace Center (DLR) and others.
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// https://www.eclipse.org/legal/epl-2.0/
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License 2.0 are satisfied: GNU General Public License, version 2
// or later which is available at
// https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
/****************************************************************************/
/// @file    MEVehicle.cpp
/// @author  Daniel Krajzewicz
/// @date    Tue, May 2005
///
// A vehicle from the mesoscopic point of view
/****************************************************************************/
#include <config.h>

#include <iostream>
#include <cassert>
#include <utils/common/StdDefs.h>
#include <utils/common/FileHelpers.h>
#include <utils/common/MsgHandler.h>
#include <utils/iodevices/OutputDevice.h>
#include <utils/xml/SUMOSAXAttributes.h>
#include <microsim/devices/MSDevice_Vehroutes.h>
#include <microsim/output/MSStopOut.h>
#include <microsim/MSGlobals.h>
#include <microsim/MSEdge.h>
#include <microsim/MSLane.h>
#include <microsim/MSNet.h>
#include <microsim/MSVehicleType.h>
#include <microsim/MSLink.h>
#include <microsim/MSStop.h>
#include <microsim/MSVehicleControl.h>
#include <microsim/transportables/MSTransportableControl.h>
#include <microsim/devices/MSDevice.h>
#include "MELoop.h"
#include "MEVehicle.h"
#include "MESegment.h"


// ===========================================================================
// method definitions
// ===========================================================================
MEVehicle::MEVehicle(SUMOVehicleParameter* pars, const MSRoute* route,
                     MSVehicleType* type, const double speedFactor) :
    MSBaseVehicle(pars, route, type, speedFactor),
    mySegment(nullptr),
    myQueIndex(0),
    myEventTime(SUMOTime_MIN),
    myLastEntryTime(SUMOTime_MIN),
    myBlockTime(SUMOTime_MAX),
    myInfluencer(nullptr) {
    if (!(*myCurrEdge)->isTazConnector()) {
        if ((*myCurrEdge)->allowedLanes(type->getVehicleClass()) == nullptr) {
            throw ProcessError("Vehicle '" + pars->id + "' is not allowed to depart on any lane of edge '" + (*myCurrEdge)->getID() + "'.");
        }
        if (pars->departSpeedProcedure == DepartSpeedDefinition::GIVEN && pars->departSpeed > type->getMaxSpeed()) {
            throw ProcessError("Departure speed for vehicle '" + pars->id +
                               "' is too high for the vehicle type '" + type->getID() + "'.");
        }
    }
}


double
MEVehicle::getBackPositionOnLane(const MSLane* /* lane */) const {
    return getPositionOnLane() - getVehicleType().getLength();
}


double
MEVehicle::getPositionOnLane() const {
// the following interpolation causes problems with arrivals and calibrators
//    const double fracOnSegment = MIN2(double(1), STEPS2TIME(MSNet::getInstance()->getCurrentTimeStep() - myLastEntryTime) / STEPS2TIME(myEventTime - myLastEntryTime));
    return mySegment == nullptr ? 0 : (double(mySegment->getIndex()) /* + fracOnSegment */) * mySegment->getLength();
}


double
MEVehicle::getAngle() const {
    const MSLane* const lane = getEdge()->getLanes()[0];
    return lane->getShape().rotationAtOffset(lane->interpolateLanePosToGeometryPos(getPositionOnLane()));
}


double
MEVehicle::getSlope() const {
    const MSLane* const lane = getEdge()->getLanes()[0];
    return lane->getShape().slopeDegreeAtOffset(lane->interpolateLanePosToGeometryPos(getPositionOnLane()));
}


Position
MEVehicle::getPosition(const double offset) const {
    const MSLane* const lane = getEdge()->getLanes()[0];
    return lane->geometryPositionAtOffset(getPositionOnLane() + offset);
}


double
MEVehicle::getSpeed() const {
    if (getWaitingTime() > 0) {
        return 0;
    } else {
        return getAverageSpeed();
    }
}


double
MEVehicle::getAverageSpeed() const {
    return mySegment->getLength() / STEPS2TIME(myEventTime - myLastEntryTime);
}


double
MEVehicle::estimateLeaveSpeed(const MSLink* link) const {
    /// @see MSVehicle.cpp::estimateLeaveSpeed
    const double v = getSpeed();
    return MIN2(link->getViaLaneOrLane()->getVehicleMaxSpeed(this),
                (double)sqrt(2 * link->getLength() * getVehicleType().getCarFollowModel().getMaxAccel() + v * v));
}


double
MEVehicle::getConservativeSpeed(SUMOTime& earliestArrival) const {
    earliestArrival = MAX2(myEventTime, earliestArrival - DELTA_T); // event times have subsecond resolution
    return mySegment->getLength() / STEPS2TIME(earliestArrival - myLastEntryTime);
}


bool
MEVehicle::moveRoutePointer() {
    // vehicle has just entered a new edge. Position is 0
    if (myCurrEdge == myRoute->end() - 1) { // may happen during teleport
        return true;
    }
    ++myCurrEdge;
    if ((*myCurrEdge)->isVaporizing()) {
        return true;
    }
    // update via
    if (myParameter->via.size() > 0 && (*myCurrEdge)->getID() == myParameter->via.front()) {
        myParameter->via.erase(myParameter->via.begin());
    }
    return hasArrived();
}



bool
MEVehicle::hasArrived() const {
    // mySegment may be 0 due to teleporting or arrival
    return myCurrEdge == myRoute->end() - 1 && (
               (mySegment == nullptr)
               || myEventTime == SUMOTime_MIN
               || getPositionOnLane() > myArrivalPos - POSITION_EPS);
}

bool
MEVehicle::isOnRoad() const {
    return getSegment() != nullptr;
}

bool
MEVehicle::isIdling() const {
    return false;
}

bool
MEVehicle::isParking() const {
    return false; // parking attribute of a stop is not yet evaluated /implemented
}


bool
MEVehicle::replaceRoute(const MSRoute* newRoute, const std::string& info,  bool onInit, int offset, bool addRouteStops, bool removeStops) {
    if (MSBaseVehicle::replaceRoute(newRoute, info, onInit, offset, addRouteStops, removeStops)) {
        if (mySegment != nullptr) {
            MSLink* const oldLink = mySegment->getLink(this);
            MSLink* const newLink = mySegment->getLink(this);
            // update approaching vehicle information
            if (oldLink != newLink) {
                if (oldLink != nullptr) {
                    oldLink->removeApproaching(this);
                }
                MELoop::setApproaching(this, newLink);
            }
        }
        return true;
    }
    return false;
}


bool
MEVehicle::isStoppedTriggered() const {
    return false;
}


bool
MEVehicle::isStoppedInRange(const double /* pos */, const double /* tolerance */) const {
    return isStopped();
}


SUMOTime
MEVehicle::checkStop(SUMOTime time) {
    for (MSStop& stop : myStops) {
        if (stop.edge != myCurrEdge || stop.segment != mySegment) {
            return time;
        }
        time += stop.duration;
        if (stop.pars.until > time) {
            // @note: this assumes the stop is reached at time. With the way this is called in MESegment (time == entryTime),
            // travel time is overestimated of the stop is not at the start of the segment
            time = stop.pars.until;
        }
        stop.reached = true;
    }
    return time;
}


double
MEVehicle::getCurrentStoppingTimeSeconds() const {
    SUMOTime time = myLastEntryTime;
    for (const MSStop& stop : myStops) {
        if (stop.reached) {
            time += stop.duration;
            if (stop.pars.until > time) {
                // @note: this assumes the stop is reached at time. With the way this is called in MESegment (time == entryTime),
                // travel time is overestimated of the stop is not at the start of the segment
                time = stop.pars.until;
            }
        } else {
            break;
        }
    }
    return STEPS2TIME(time - myLastEntryTime);
}


void
MEVehicle::processStop() {
    assert(isStopped());
    MSEdge* edge = const_cast<MSEdge*>(getEdge());
    double lastPos = 0;
    for (auto it = myStops.begin(); it != myStops.end();) {
        MSStop& stop = *it;
        if (stop.edge != myCurrEdge || stop.segment != mySegment || stop.pars.endPos <= lastPos) {
            break;
        }
        lastPos = stop.pars.endPos;
        stop.pars.actualArrival = myLastEntryTime;
        if (MSStopOut::active()) {
            MSStopOut::getInstance()->stopStarted(this, getPersonNumber(), getContainerNumber(), myLastEntryTime);
        }
        MSNet* const net = MSNet::getInstance();
        SUMOTime dummy = -1; // boarding- and loading-time are not considered
        if (net->hasPersons()) {
            net->getPersonControl().boardAnyWaiting(edge, this, stop.pars, dummy, dummy);
        }
        if (net->hasContainers()) {
            net->getContainerControl().loadAnyWaiting(edge, this, stop.pars, dummy, dummy);
        }
        MSDevice_Vehroutes* vehroutes = static_cast<MSDevice_Vehroutes*>(getDevice(typeid(MSDevice_Vehroutes)));
        if (vehroutes != nullptr) {
            vehroutes->stopEnded(stop.pars);
        }
        if (MSStopOut::active()) {
            MSStopOut::getInstance()->stopEnded(this, stop.pars, mySegment->getEdge().getID());
        }
        SUMOVehicleParameter::Stop pars = stop.pars;
        pars.depart = MSNet::getInstance()->getCurrentTimeStep();
        myPastStops.emplace_back(pars);
        it = myStops.erase(it);
    }
    mySegment->getEdge().removeWaiting(this);
}


bool
MEVehicle::mayProceed() const {
    return mySegment == nullptr || mySegment->isOpen(this);
}


double
MEVehicle::getCurrentLinkPenaltySeconds() const {
    if (mySegment == nullptr) {
        return 0;
    } else {
        return STEPS2TIME(mySegment->getLinkPenalty(this));
    }
}


void
MEVehicle::updateDetectorForWriting(MSMoveReminder* rem, SUMOTime currentTime, SUMOTime exitTime) {
    for (MoveReminderCont::iterator i = myMoveReminders.begin(); i != myMoveReminders.end(); ++i) {
        if (i->first == rem) {
            rem->updateDetector(*this, mySegment->getIndex() * mySegment->getLength(),
                                (mySegment->getIndex() + 1) * mySegment->getLength(),
                                getLastEntryTime(), currentTime, exitTime, false);
#ifdef _DEBUG
            if (myTraceMoveReminders) {
                traceMoveReminder("notifyMove", i->first, i->second, true);
            }
#endif
            return;
        }
    }
}


void
MEVehicle::updateDetectors(SUMOTime currentTime, const bool isLeave, const MSMoveReminder::Notification reason) {
    // segments of the same edge have the same reminder so no cleaning up must take place
    if (reason == MSMoveReminder::NOTIFICATION_JUNCTION || reason == MSMoveReminder::NOTIFICATION_TELEPORT) {
        myOdometer += getEdge()->getLength();
    }
    const bool cleanUp = isLeave && (reason != MSMoveReminder::NOTIFICATION_SEGMENT);
    for (MoveReminderCont::iterator rem = myMoveReminders.begin(); rem != myMoveReminders.end();) {
        if (currentTime != getLastEntryTime()) {
            rem->first->updateDetector(*this, mySegment->getIndex() * mySegment->getLength(),
                                       (mySegment->getIndex() + 1) * mySegment->getLength(),
                                       getLastEntryTime(), currentTime, getEventTime(), cleanUp);
#ifdef _DEBUG
            if (myTraceMoveReminders) {
                traceMoveReminder("notifyMove", rem->first, rem->second, true);
            }
#endif
        }
        if (!isLeave || rem->first->notifyLeave(*this, mySegment->getLength(), reason)) {
#ifdef _DEBUG
            if (isLeave && myTraceMoveReminders) {
                traceMoveReminder("notifyLeave", rem->first, rem->second, true);
            }
#endif
            ++rem;
        } else {
#ifdef _DEBUG
            if (myTraceMoveReminders) {
                traceMoveReminder("remove", rem->first, rem->second, false);
            }
#endif
            rem = myMoveReminders.erase(rem);
        }
    }
}


MEVehicle::BaseInfluencer&
MEVehicle::getBaseInfluencer() {
    if (myInfluencer == nullptr) {
        myInfluencer = new BaseInfluencer();
    }
    return *myInfluencer;
}


const MEVehicle::BaseInfluencer*
MEVehicle::getBaseInfluencer() const {
    return myInfluencer;
}


void
MEVehicle::onRemovalFromNet(const MSMoveReminder::Notification reason) {
    MSGlobals::gMesoNet->removeLeaderCar(this);
    MSGlobals::gMesoNet->changeSegment(this, MSNet::getInstance()->getCurrentTimeStep(), nullptr, reason);
}

void
MEVehicle::saveState(OutputDevice& out) {
    if (mySegment != nullptr && MESegment::isInvalid(mySegment)) {
        // segment is vaporization target, do not write this vehicle
        return;
    }
    MSBaseVehicle::saveState(out);
    assert(mySegment == nullptr || *myCurrEdge == &mySegment->getEdge());
    std::vector<SUMOTime> internals;
    internals.push_back(myDeparture);
    internals.push_back((SUMOTime)distance(myRoute->begin(), myCurrEdge));
    internals.push_back((SUMOTime)myDepartPos * 1000); // store as mm
    internals.push_back(mySegment == nullptr ? (SUMOTime) - 1 : (SUMOTime)mySegment->getIndex());
    internals.push_back((SUMOTime)getQueIndex());
    internals.push_back(myEventTime);
    internals.push_back(myLastEntryTime);
    internals.push_back(myBlockTime);
    out.writeAttr(SUMO_ATTR_STATE, toString(internals));
    // save past stops
    for (SUMOVehicleParameter::Stop stop : myPastStops) {
        stop.write(out, false);
        out.writeAttr("actualArrival", time2string(stop.actualArrival));
        out.writeAttr(SUMO_ATTR_DEPART, time2string(stop.depart));
        out.closeTag();
    }
    // save upcoming stops
    for (const MSStop& stop : myStops) {
        stop.write(out);
    }
    // save parameters
    myParameter->writeParams(out);
    for (MSDevice* dev : myDevices) {
        dev->saveState(out);
    }
    out.closeTag();
}


void
MEVehicle::loadState(const SUMOSAXAttributes& attrs, const SUMOTime offset) {
    if (attrs.hasAttribute(SUMO_ATTR_POSITION)) {
        throw ProcessError("Error: Invalid vehicles in state (may be a micro state)!");
    }
    int routeOffset;
    int segIndex;
    int queIndex;
    std::istringstream bis(attrs.getString(SUMO_ATTR_STATE));
    bis >> myDeparture;
    bis >> routeOffset;
    bis >> myDepartPos;
    bis >> segIndex;
    bis >> queIndex;
    bis >> myEventTime;
    bis >> myLastEntryTime;
    bis >> myBlockTime;
    myDepartPos *= 1000; // was stored as mm
    if (hasDeparted()) {
        myDeparture -= offset;
        myEventTime -= offset;
        myLastEntryTime -= offset;
        myCurrEdge += routeOffset;
        if (segIndex >= 0) {
            MESegment* seg = MSGlobals::gMesoNet->getSegmentForEdge(**myCurrEdge);
            while (seg->getIndex() != (int)segIndex) {
                seg = seg->getNextSegment();
                assert(seg != 0);
            }
            setSegment(seg, queIndex);
        } else {
            // on teleport
            setSegment(nullptr, 0);
            assert(myEventTime != SUMOTime_MIN);
            MSGlobals::gMesoNet->addLeaderCar(this, nullptr);
        }
    }
    if (myBlockTime != SUMOTime_MAX) {
        myBlockTime -= offset;
    }
}


/****************************************************************************/
