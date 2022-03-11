#!/bin/sh
cd /home/pi/iot_domoticz
until sudo node ./mqtt_Cluster.js >> /home/pi/Samba/domoticz/mqtt_Cluster.log 2>> /home/pi/Samba/domoticz/mqtt_Cluster.log; do
echo "Server 'mqtt_Cluster' crashed with exit code $?. Respawning.." >> /home/pi/Samba/domoticz/mqtt_Cluster.log
sleep 1
done
