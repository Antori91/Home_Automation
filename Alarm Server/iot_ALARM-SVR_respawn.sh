#!/bin/sh
# "./iot_ALARM-SVR_respawn.sh &" to run this script
cd /home/pi/iot_domoticz
until node ./iot_ALARM-SVR.js >> /home/pi/Samba/iot_ALARM-SVR.log 2>> /home/pi/Samba/iot_ALARM-SVR.log; do
echo "Server 'iot_ALARM-SVR' crashed with exit code $?. Respawning.." >&2
sleep 1
done

