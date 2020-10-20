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
/// @file    GNEMove.h
/// @author  Pablo Alvarez Lopez
/// @date    Mar 2020
///
// Class used for elements that own a movable shape
/****************************************************************************/
#pragma once
#include <config.h>

#include "GNEGeometry.h"

// ===========================================================================
// class declaration
// ===========================================================================

class GNEMoveElement;
class GNEUndoList;

// ===========================================================================
// class definitions
// ===========================================================================

/// @brief move operation
class GNEMoveOperation {

public:
    /// @brief constructor for values with a single position (junctions, E3, ParkingSpaces...)
    GNEMoveOperation(GNEMoveElement *moveElement,
        const Position originalPosition);

    /// @brief constructor for entire geometries (Polygon with blocked shapes)
    GNEMoveOperation(GNEMoveElement *moveElement, 
        const PositionVector originalShape);

    /// @brief constructor for elements with editable shapes (edges, polygons...)
    GNEMoveOperation(GNEMoveElement *moveElement, 
        const PositionVector originalShape,
        const std::vector<int> originalgeometryPoints,
        const PositionVector shapeToMove,
        const std::vector<int> geometryPointsToMove);

    /// @brief constructor for elements placed over lanes (StoppingPlaces, detectors...)
    GNEMoveOperation(GNEMoveElement *moveElement, 
        const GNELane* lane,
        const std::vector<double> originalPosOverLanes);

    /// @brief constructor for edit elements placed over lanes (start/end of StoppingPlaces, detectors...)
    GNEMoveOperation(GNEMoveElement *moveElement, 
        const GNELane* lane,
        const std::vector<double> originalPosOverLanes,
        const std::vector<int> geometryPointsToMove);

    /// @brief destructor
    ~GNEMoveOperation();

    /// @brief move element
    GNEMoveElement *moveElement;

    /// @brief original shape
    const PositionVector originalShape;

    /// @brief original shape points to move (of original shape)
    const std::vector<int> originalGeometryPoints;

    /**@brief shape to move
     * @note: it can be different of originalShape, for example due a new geometry point
     */
    const PositionVector shapeToMove;

    /// @brief shape points to move (of shapeToMove)
    const std::vector<int> geometryPointsToMove;

    /// @brief original lane
    const GNELane* lane;

    /// @brief original position over lanes
    const std::vector<double> originalPosOverLanes;

private:
    /// @brief Invalidated copy constructor.
    GNEMoveOperation(const GNEMoveOperation&) = delete;

    /// @brief Invalidated assignment operator.
    GNEMoveOperation& operator=(const GNEMoveOperation&) = delete;
};


/// @brief move result
class GNEMoveResult {

public:
    /// @brief constructor
    GNEMoveResult();

    /// @brief destructor
    ~GNEMoveResult();

    /// @brief shape to update (edited in moveElement)
    PositionVector shapeToUpdate;

    /// @brief shape points to move (of shapeToMove)
    std::vector<int> geometryPointsToMove;

private:
    /// @brief Invalidated copy constructor.
    GNEMoveResult(const GNEMoveResult&) = delete;
};


/// @brief move element
class GNEMoveElement {

public:
    /// @brief constructor
    GNEMoveElement();

    /**@brief get move operation for the given shapeOffset
     * @note returned GNEMoveOperation can be nullptr
     */
    virtual GNEMoveOperation* getMoveOperation(const double shapeOffset) = 0;

    /// @brief remove geometry point in the clicked position
    virtual void removeGeometryPoint(const Position clickedPosition, GNEUndoList* undoList) = 0;

    /// @brief move element the for given offset (note: offset can be X-Y-0, 0-0-Z or X-Y-Z)
    static void moveElement(const GNEViewNet* viewNet, GNEMoveOperation* moveOperation, const Position &offset);

    /// @brief commit move element for the given offset
    static void commitMove(const GNEViewNet* viewNet, GNEMoveOperation* moveOperation, const Position &offset, GNEUndoList* undoList);

private:
    /// @brief set move shape
    virtual void setMoveShape(const GNEMoveResult& moveResult) = 0;

    /// @brief commit move shape
    virtual void commitMoveShape(const GNEMoveResult& moveResult, GNEUndoList* undoList) = 0;

    /// @brief calculate movement over lane
    static const PositionVector calculateMovementOverLane(const GNEViewNet* viewNet, const GNEMoveOperation* moveOperation, const Position &offset);

    /// @brief Invalidated copy constructor.
    GNEMoveElement(const GNEMoveElement&) = delete;

    /// @brief Invalidated assignment operator.
    GNEMoveElement& operator=(const GNEMoveElement&) = delete;
};
