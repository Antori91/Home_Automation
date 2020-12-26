#!/bin/sh
cd /home/pi/iot_domoticz
until node ./mqtt_Cluster.js >> /home/pi/Samba/domoticz/mqtt_Cluster.log 2>> /home/pi/Samba/domoticz/mqtt_Cluster.log; do
echo "Server 'mqtt_Cluster' crashed with exit code $?. Respawning.." >&2
sleep 1
done