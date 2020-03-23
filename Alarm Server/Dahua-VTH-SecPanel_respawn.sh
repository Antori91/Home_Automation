#!/bin/sh
# "./Dahua-VTH-SecPanel_respawn &" to run this script
cd /home/pi/iot_domoticz
until python3 ./Dahua-VTH-SecPanel.py >> /home/pi/Samba/domoticz/Dahua-VTH-SecPanel.log 2>> /home/pi/Samba/domoticz/Dahua-VTH-SecPanel.log; do
echo "Server 'Dahua-VTH-SecPanel' crashed with exit code $?. Respawning.." >&2
sleep 1
done

