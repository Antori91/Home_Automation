#!/bin/sh
cd /home/pi/iot_domoticz
until node ./iot_ESP8266.js RASPBERRY >> /home/pi/Samba/domoticz/iot_ESP8266.log 2>> /home/pi/Samba/domoticz/iot_ESP8266.log; do
echo "Server 'iot_ESP8266' crashed with exit code $?. Respawning.." >&2
sleep 1
done