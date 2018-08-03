#!/bin/sh
cd /volume1/@appstore/iot_domoticz
cp /volume1/@appstore/domoticz/var/backups/hourly/backup-hour-23.db /volume1/sauvegarde/ONEDRIVE_DOMOTICZ
echo -e "==\n== LAST DOMOTICZ ACTIVITIES ==\n=="  > /volume1/Alarme/DomoticZ/domoticzEvents.log
tail /volume1/@appstore/domoticz/var/domoticz.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
echo -e "==\n== MAJOR DEVICE EVENTS ==\n=="  >> /volume1/Alarme/DomoticZ/domoticzEvents.log
/bin/grep -i -a -e DHT22 -e webserver -e opening -e Heater -e Lighting -e ALARM -e HotWaterTank -e incoming -e login /volume1/@appstore/domoticz/var/domoticz.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
echo -e "==\n== ALARM SERVER/ERROR EVENTS ==\n==" >> /volume1/Alarme/DomoticZ/domoticzEvents.log
/bin/grep -i -e FAILURE /volume1/Alarme/RaspberrySamba/iot_ALARM-SVR.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
echo -e "==\n== DECOMMISSIONED PUBNUB ALARM SERVER/PUBNUB WARNING/ERROR EVENTS ==\n==" >> /volume1/Alarme/DomoticZ/domoticzEvents.log
/bin/grep -h -r -e RECONNECT /volume1/Alarme/iot_CVQ6081-SecPanel >> /volume1/Alarme/DomoticZ/domoticzEvents.log
/bin/grep -i -e ERROR /volume1/Alarme/RaspberrySamba/iot_CVQ6081.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
cat /volume1/Alarme/RaspberrySamba/*.pubnub.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
logrotate ./DomoticzLogRotate.conf -v -f

