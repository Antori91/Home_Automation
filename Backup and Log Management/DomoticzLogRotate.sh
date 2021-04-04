#!/bin/sh
# @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
# ***** Domoticz database backup and log management *****
# V1.0 - April 2020    - Dahua VTH Security Panel log added
# V0.95- May 2019      - Main server IP changed to 127.0.0.1
# V0.9 - April 2019    - Grep in Domoticz logfile MSG instead of Heater, Lighting, ALARM and HotWaterTank
#                      - Tail -n25 instead of cat the Domoticz and motionEye connections
#                      - Grep also dzSYNC in Alarm server logfile
# V0.8 - March 2019    - User/password now mandatory to access Backup Domoticz server (basic authentication)
#                      - Domoticz cluster and motionEye reverse proxy incoming connections added
# V0.7 - January 2019  - motionEye incoming connections added
# V0.6 - December 2018 - Domoticz automatic backup not used anymore. 
MAIN_SERVER_IP="127.0.0.1"                 # Main Domoticz server
BACKUP_SERVER_IP="192.168.x.xxx"        # Backup Domoticz server
DZ_PORT="9999"                          # Domoticz IP port 
BACKUP_SERVER_USER="-------"            # Domoticz backup server user name
BACKUP_SERVER_PWD="-------------"       # Domoticz backup server user password
MAIN_DZ_BACKUPFILE="/volume1/Alarme/Dz_DBase_Backup/MainDzSvr.db"
BACKUP_DZ_BACKUPFILE="/volume1/Alarme/Dz_DBase_Backup/BackupDzSvr.db"          
curl -s "http://$MAIN_SERVER_IP:$DZ_PORT/json.htm?type=command&param=addlogmessage&message=Starting%20manual%20database%20backup%20procedure..."
curl -s -u $BACKUP_SERVER_USER:$BACKUP_SERVER_PWD "http://$BACKUP_SERVER_IP:$DZ_PORT/json.htm?type=command&param=addlogmessage&message=Starting%20manual%20database%20backup%20procedure..."
sleep 1
curl -s http://$MAIN_SERVER_IP:$DZ_PORT/backupdatabase.php   > $MAIN_DZ_BACKUPFILE
curl -s -u $BACKUP_SERVER_USER:$BACKUP_SERVER_PWD http://$BACKUP_SERVER_IP:$DZ_PORT/backupdatabase.php > $BACKUP_DZ_BACKUPFILE
sleep 1
curl -s "http://$MAIN_SERVER_IP:$DZ_PORT/json.htm?type=command&param=addlogmessage&message=Ending%20manual%20database%20backup%20procedure..."
curl -s -u $BACKUP_SERVER_USER:$BACKUP_SERVER_PWD "http://$BACKUP_SERVER_IP:$DZ_PORT/json.htm?type=command&param=addlogmessage&message=Ending%20manual%20database%20backup%20procedure..."
cp $MAIN_DZ_BACKUPFILE   /volume1/sauvegarde/ONEDRIVE_DOMOTICZ #Synology Cloud Sync doesn't work correctly if you do the curl within ONEDRIVE_DOMOTICZ directory
cp $BACKUP_DZ_BACKUPFILE /volume1/sauvegarde/ONEDRIVE_DOMOTICZ
cd /volume1/@appstore/iot_domoticz
echo -e "==\n== LAST MAIN DOMOTICZ SERVER ACTIVITIES ==\n=="  > /volume1/Alarme/DomoticZ/domoticzEvents.log
tail /volume1/@appstore/domoticz/var/domoticz.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
echo -e "==\n== MAJOR MAIN DOMOTICZ and DEVICE EVENTS ==\n=="  >> /volume1/Alarme/DomoticZ/domoticzEvents.log
/bin/grep -i -a -e DHT22 -e webserver -e opening -e MSG /volume1/@appstore/domoticz/var/domoticz.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
echo -e "==\n== CLUSTER WARNING/ERROR EVENTS ==\n==" >> /volume1/Alarme/DomoticZ/domoticzEvents.log
/bin/grep -i -a -e FAILURE -e ALERT /volume1/Alarme/mtCluster_DMOTIC_BACKUP/domoticz/mqtt_Cluster.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
echo -e "==\n== DOMOTICZ CLUSTER INCOMING CONNECTIONS ==\n==" >> /volume1/Alarme/DomoticZ/domoticzEvents.log
tail -n25 /var/log/nginx/Dz.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
/bin/grep -i -a -e incoming -e login /volume1/@appstore/domoticz/var/domoticz.log | tail -n25 >> /volume1/Alarme/DomoticZ/domoticzEvents.log
echo -e "==\n== MOTIONEYE INCOMING CONNECTIONS ==\n==" >> /volume1/Alarme/DomoticZ/domoticzEvents.log
tail -n25 /var/log/nginx/mEye.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
cat /volume1/Alarme/RaspberrySamba/iptables.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
echo -e "==\n== ALARM SERVER WARNING/ERROR EVENTS ==\n==" >> /volume1/Alarme/DomoticZ/domoticzEvents.log
/bin/grep -i -e FAILURE -e dzSYNC /volume1/Alarme/RaspberrySamba/iot_ALARM-SVR.log | tail -n25 >> /volume1/Alarme/DomoticZ/domoticzEvents.log
/bin/grep -i -a -e "\[-]" -e "\[!]" /volume1/Alarme/mtCluster_DMOTIC_BACKUP/domoticz/Dahua-VTH-SecPanel.log | tail -n25 >> /volume1/Alarme/DomoticZ/domoticzEvents.log
# echo -e "==\n== DECOMMISSIONED PUBNUB ALARM SERVER/PUBNUB WARNING/ERROR EVENTS ==\n==" >> /volume1/Alarme/DomoticZ/domoticzEvents.log
# /bin/grep -h -r -e RECONNECT /volume1/Alarme/iot_CVQ6081-SecPanel >> /volume1/Alarme/DomoticZ/domoticzEvents.log
# /bin/grep -i -e ERROR /volume1/Alarme/RaspberrySamba/iot_CVQ6081.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
# cat /volume1/Alarme/RaspberrySamba/*.pubnub.log >> /volume1/Alarme/DomoticZ/domoticzEvents.log
logrotate ./DomoticzLogRotate.conf -v

