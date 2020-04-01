#!/bin/sh
cd /volume1/@appstore/iot_domoticz
until node ./iot_Orchestrator.js; do
echo "Server 'iot_Orchestrator' crashed with exit code $?. Respawning.." >&2
sleep 1
done
