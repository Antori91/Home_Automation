#!/bin/sh
cd /home/pi/iot_domoticz
node ./iot_Orchestrator.js RASPBERRY > /home/pi/Samba/domoticz/iot_Orchestrator.log 2> /home/pi/Samba/domoticz/iot_Orchestrator.log &
