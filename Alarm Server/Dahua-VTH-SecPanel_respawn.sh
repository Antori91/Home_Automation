#!/bin/sh
# "./Dahua-VTH-SecPanel_respawn &" to run this script
cd /home/pi/iot_domoticz
sudo tcpdump --number -n -l 'tcp[tcpflags] & tcp-syn != 0 and dst 192.168.X.XXX' > /home/pi/iot_domoticz/Dahua-VTH-BOX-cxion.log &
#tcpdump_PID=$!
until python3 ./Dahua-VTH-SecPanel.py >> /home/pi/Samba/domoticz/Dahua-VTH-SecPanel.log 2>> /home/pi/Samba/domoticz/Dahua-VTH-SecPanel.log; do
echo "Server 'Dahua-VTH-SecPanel' crashed with exit code $?. Respawning.." >&2
sleep 1
done
#sudo kill -9 $tcpdump_PID
