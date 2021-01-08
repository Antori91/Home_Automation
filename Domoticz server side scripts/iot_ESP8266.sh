#!/bin/sh
cd /volume1/@appstore/iot_domoticz
until node ./iot_ESP8266.js; do
echo "Server 'iot_ESP8266' crashed with exit code $?. Respawning.." >&2
sleep 1
done