#!/bin/sh
cd /home/pi/iot_domoticz
node ./mqtt_Cluster.js > /home/pi/Samba/domoticz/mqtt_Cluster.log 2> /home/pi/Samba/domoticz/mqtt_Cluster.log &
