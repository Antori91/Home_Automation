#!/bin/sh
# "./iot_Orchestrator_RBY.sh &" to run this script
cd /home/pi/iot_domoticz
until node ./iot_Orchestrator.js RASPBERRY >> /home/pi/Samba/domoticz/iot_Orchestrator.log 2>> /home/pi/Samba/domoticz/iot_Orchestrator.log; do
echo "Server 'iot_Orchestrator' crashed with exit code $?. Respawning.." >&2
sleep 1
done

