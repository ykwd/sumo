import os, sys

if 'SUMO_HOME' in os.environ:
    tools = os.path.join(os.environ['SUMO_HOME'], 'tools')
    sys.path.append(tools)
else:
    sys.exit("please declare environment variable 'SUMO_HOME'")


import sumolib
import traci
import traci.constants as tc

traci.start([sumolib.checkBinary("sumo"), "-c", "data/OnlyNetQuickstart.sumocfg", "--no-step-log"])

traci.vehicle.add(vehID="bus_1", typeID="BUS", routeID="", depart=0, departPos=0, departSpeed=0, departLane=1)
traci.vehicle.setRoute("bus_1", ["L12", "L15", "L5",])
traci.vehicle.setStop(vehID="bus_1", edgeID="bus_stop_1", pos=0, laneIndex=1, duration=50, flags=tc.STOP_BUS_STOP)
traci.vehicle.setStop(vehID="bus_1", edgeID="bus_stop_2", pos=0, laneIndex=1, duration=50, flags=tc.STOP_BUS_STOP)

traci.person.add(personID="P4", edgeID="L12", pos=15, depart=0, typeID='DEFAULT_PEDTYPE')
stage = traci.simulation.Stage(type=tc.STAGE_DRIVING, line="ANY", edges=["L5"], departPos=0, arrivalPos=79.34, destStop='bus_stop_2', description="foo")
traci.person.appendStage("P4", stage)
traci.person.appendWaitingStage(personID="P4", duration=1000, description='thinking', stopID='bus_stop_2')

traci.person.add(personID="P5", edgeID="L12", pos=15, depart=20, typeID='DEFAULT_PEDTYPE')
stage = traci.simulation.Stage(type=tc.STAGE_DRIVING, line="ANY", edges=["L5"], departPos=0, destStop='bus_stop_2')
traci.person.appendStage("P5", stage)
traci.person.appendWaitingStage(personID="P5", duration=1000, description='thinking', stopID='bus_stop_2')

step = 0
while step < 1500:
   traci.simulationStep()
   step += 1

traci.close()
