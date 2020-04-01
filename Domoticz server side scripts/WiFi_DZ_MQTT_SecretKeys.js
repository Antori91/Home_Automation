// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// V0.6 - March 2020
// Dahua VTO/H and Alarm stationID added
// V0.5 - April 2019
// Fire Alarm IDX added
// V0.4 - February 2019
// Cluster Main server SSH Port/User/Pwd added 
// V0.3 - August 2018 

var mySecretKeys = {
MAIN_SERVER_HARDWARE       : 'SYNOLOGY',      // Main Domoticz server Hardware: SYNOLOGY and RASPBERRY supported
MAIN_SERVER_IP             : '192.168.Y.YYY', // Main Domoticz server IP address
MAIN_SERVER_SSH_PORT       :  YYYY,           // Main Domoticz server SSH Port
MAIN_SERVER_SSH_USER       : '---------',     // Main Domoticz server SSH User
MAIN_SERVER_ROOT_PWD       : '-----------',   // Main Domoticz server root Password
BACKUP_SERVER_HARDWARE     : 'RASPBERRY',     // Backup Domoticz server Hardware
BACKUP_SERVER_IP           : '192.168.Y.YYY', // Backup Domoticz server IP address
DZ_PORT                    :  YYYY,           // Domoticz IP port         
DZ_IN_TOPIC                : '-----------',   // Domoticz In mqtt topic
DZ_OUT_TOPIC               : '-----------',   // Domoticz Out mqtt topic
MQTT_PORT                  :  YYYY,           // Mqtt IP port         
idxClusterFailureFlag      : 'YY',                               //[$$DOMOTICZ_PARAMETER] - General Home Automation Failure status (i.e. Hardware Server, Dz, Mqtt, ... ) - Dz "Panne Domotique" Device
idx_AlarmFailureFlag       : 'YY',                               //[$$DOMOTICZ_PARAMETER] - Alarm Failure status - Dz "Panne Alarme" Device
idx_TempFailureFlag        : 'YY',                               //[$$DOMOTICZ_PARAMETER] - Temp sensors Failure status - Dz "Panne Sondes Temperature" Device 
idx_HeatingSelector        : 'YY',                               //[$$DOMOTICZ_PARAMETER] - Dz Global heating breaker (OFF/HORSGEL/ECO/CONFORT) Device
idx_UnactiveHeatersDisplay : 'YY',                               //[$$DOMOTICZ_PARAMETER] - Dz "Zones Chauffage Actives" Device
idx_ActivateHeaters        : 'YY',                               //[$$DOMOTICZ_PARAMETER] - Dz "Horaires/Start Chauffage" Device
idx_UnactivateHeaters      : 'YY',                               //[$$DOMOTICZ_PARAMETER] - Dz "Horaires/Stop Chauffage" Device
Var_UnactiveHeaters        : 'Y',                                //[$$DOMOTICZ_PARAMETER] - Dz user variable# to store the heaters heating state command
Varname_UnactiveHeaters    : 'HeatersActive',                    //[$$DOMOTICZ_PARAMETER] - Dz user variable name to store the heaters heating state command
ProtectedDevicePassword    : '----',                             //[$$DOMOTICZ_PARAMETER] - Dz password for protected device
idx_SecPanel               : 'YY',                               //                       - Alarm Server Security Panel (MD5 signed message)
DZ_idx_SecPanel            :  YY,                                //[$$DOMOTICZ_PARAMETER] - Dz Security Panel
SecPanel_Seccode           : '--------------------------------', //[$$DOMOTICZ_PARAMETER] - Dz Security Panel Password MD5 Encrypted 
idx_AlarmALERT             : YY,                                 //[$$DOMOTICZ_PARAMETER] - Alarm Alert status - Dz "ALERTE Intrusion Maison" Device 
idx_AlarmARM               : YY,                                 //[$$DOMOTICZ_PARAMETER] - Alarm Armed status - Dz "Maison sous Alarme" Device
idx_FireALarmStatus        : YY,                                 //[$$DOMOTICZ_PARAMETER] - Fire Alarm device IDX (Selector)
idx_FireAlarmTempHum       : YY,                                 //[$$DOMOTICZ_PARAMETER] - Fire Alarm temp/humidity device IDX
idx_FireAlarmHeatidx       : YY,                                 //[$$DOMOTICZ_PARAMETER] - Fire Alarm temp heat index device IDX
SVR_ALARM_SID              : '-----------',  // Alarm Server network SID
DZ_ALARM_CID               : '-----------',  // Dz Alarm network CID
VTH_ALARM_CID              : '-----------',  // VTH Alarm network CID
VTO_ALARM_CID              : '-----------'   // VTO Alarm network CID
};

module.exports   = mySecretKeys;