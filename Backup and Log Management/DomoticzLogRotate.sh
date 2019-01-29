#!/bin/sh
# @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
# ***** Domoticz database backup and log management *****
# V0.7 - January 2019 - motionEye incoming connections added
# V0.6 - December 2018 - Domoticz automatic backup not used anymore. 
MAIN_SERVER_IP="192.168.1.XXX"    # Main Domoticz server
BACKUP_SERVER_IP="192.168.1.XXX" # Backup Domoticz server
DZ_PORT="XXXX"                   # Domoticz IP port 
MAIN_DZ_BACKUPFILE="/volume1/Alarme/Dz_DBase_Backup/MainDzSvr.db"
BACKUP_DZ_BACKUPFILE="/volume1/Alarme/Dz_DBase_Backup/BackupDzSvr.db"          
curl -s "http://$MAIN_SERVER_IP:$DZ_PORT/json.htm?type=command&param=addlogmessage&message=Starting%20manual%20database%20backup%20procedure..."
curl -s "http://$BACKUP_SERVER_IP:$DZ_PORT/json.htm?type=command&param=addlogmessage&message=Starting%20manual%20database%20backup%20procedure..."
sleep 1
curl -s http://$MAIN_SERVER_IP:$DZ_PORT/backupdatabase.php   > $MAIN_DZ_BACKUPFILE
curl -s http://$BACKUP_SERVER_IP:$DZ_PORT/backupdatabase.php > $BACKUP_DZ_BACKUPFILE
sleep 1
curl -s "http://$MAIN_SERVER_IP:$DZ_PORT/json.htm?type=command&param=addlogmessage&message=Ending%20manual%20database%20backup%20procedure..."
curl -s "http://$BACKUP_SERVER_IP:$DZ_PORT/json.htm?type=command&param=addlogmessage&message=Ending%20manual%20database%20backup%20procedure..."
cp $MAIN_DZ_BACKUPFILE   /volume1/sauvegarde/ONEDRIVE_DOMOTICZ #Synology Cloud Sync doesn't work correctly if you do the curl within ONEDRIVE_DOMOTICZ directory
cp $BACKUP_DZ_BACKUPFILE /volume1/sauvegarde/ONEDRIVE_DOMOTICZ
cd /volume1/@appstore/iot_domoticz
echo -e "==\n== LAST MAIN DOMOTICZ SERVER ACTIVITIES ==\n=="  > /volume1/Alarme/DomoticZ/domoticzEvents.log
tail /volume1/@appstore/domoticz/var/domoticz.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
echo -e "==\n== MAJOR MAIN DOMOTICZ and DEVICE EVENTS ==\n=="  >> /volume1/Alarme/DomoticZ/domoticzEvents.log
/bin/grep -i -a -e DHT22 -e webserver -e opening -e Heater -e Lighting -e ALARM -e HotWaterTank -e incoming -e login /volume1/@appstore/domoticz/var/domoticz.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
echo -e "==\n== CLUSTER WARNING/ERROR EVENTS ==\n==" >> /volume1/Alarme/DomoticZ/domoticzEvents.log
/bin/grep -i -e FAILURE -e ALERT /volume1/Alarme/mtCluster_DMOTIC_BACKUP/domoticz/mqtt_Cluster.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
echo -e "==\n== ALARM SERVER WARNING/ERROR EVENTS ==\n==" >> /volume1/Alarme/DomoticZ/domoticzEvents.log
/bin/grep -i -e FAILURE /volume1/Alarme/RaspberrySamba/iot_ALARM-SVR.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
echo -e "==\n== MOTIONEYE INCOMING CONNECTIONS ==\n==" >> /volume1/Alarme/DomoticZ/domoticzEvents.log
cat /volume1/Alarme/RaspberrySamba/iptables.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
echo -e "==\n== DECOMMISSIONED PUBNUB ALARM SERVER/PUBNUB WARNING/ERROR EVENTS ==\n==" >> /volume1/Alarme/DomoticZ/domoticzEvents.log
/bin/grep -h -r -e RECONNECT /volume1/Alarme/iot_CVQ6081-SecPanel >> /volume1/Alarme/DomoticZ/domoticzEvents.log
/bin/grep -i -e ERROR /volume1/Alarme/RaspberrySamba/iot_CVQ6081.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
cat /volume1/Alarme/RaspberrySamba/*.pubnub.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
logrotate ./DomoticzLogRotate.conf -v -f

