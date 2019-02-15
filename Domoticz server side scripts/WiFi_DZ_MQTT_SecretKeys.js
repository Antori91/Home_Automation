// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// V0.4 - February 2019
// Cluster Main server SSH Port/User/Pwd added 
// V0.3 - August 2018 

var mySecretKeys = {
MAIN_SERVER_HARDWARE       : 'SYNOLOGY',      // Main Domoticz server Hardware: SYNOLOGY and RASPBERRY supported
MAIN_SERVER_IP             : '192.168.1.XX',  // Main Domoticz server IP address
MAIN_SERVER_SSH_PORT       :  XXX,            // Main Domoticz server SSH Port
MAIN_SERVER_SSH_USER       : '----------',    // Main Domoticz server SSH User
MAIN_SERVER_ROOT_PWD       : '----------',    // Main Domoticz server root Password
BACKUP_SERVER_HARDWARE     : 'RASPBERRY',     // Backup Domoticz server Hardware
BACKUP_SERVER_IP           : '192.168.1.XXX', // Backup Domoticz server IP address
DZ_PORT                    :  XXXX,           // Domoticz IP port         
idxClusterFailureFlag      : '46',                               //[$$DOMOTICZ_PARAMETER] - General Home Automation Failure status (i.e. Hardware Server, Dz, Mqtt, ... ) - Dz "Panne Domotique" Device
idx_AlarmFailureFlag       : '42',                               //[$$DOMOTICZ_PARAMETER] - Alarm Failure status - Dz "Panne Alarme" Device
idx_TempFailureFlag        : '43',                               //[$$DOMOTICZ_PARAMETER] - Temp sensors Failure status - Dz "Panne Sondes Temperature" Device 
idx_HeatingSelector        : '17',                               //[$$DOMOTICZ_PARAMETER] - Dz Global heating breaker (OFF/HORSGEL/ECO/CONFORT) Device
idx_UnactiveHeatersDisplay : '47',                               //[$$DOMOTICZ_PARAMETER] - Dz "Zones Chauffage Actives" Device
idx_ActivateHeaters        : '48',                               //[$$DOMOTICZ_PARAMETER] - Dz "Horaires/Start Chauffage" Device
idx_UnactivateHeaters      : '23',                               //[$$DOMOTICZ_PARAMETER] - Dz "Horaires/Stop Chauffage" Device
Var_UnactiveHeaters        : '1',                                //[$$DOMOTICZ_PARAMETER] - Dz user variable# to store the heaters heating state command
Varname_UnactiveHeaters    : 'HeatersActive',                    //[$$DOMOTICZ_PARAMETER] - Dz user variable name to store the heaters heating state command
ProtectedDevicePassword    : 'XXXX',                             //[$$DOMOTICZ_PARAMETER] - Dz password for protected device
idx_SecPanel               : 'X',                                //[$$DOMOTICZ_PARAMETER] - Dz Security Panel
SecPanel_Seccode           : '------------------------------',   //[$$DOMOTICZ_PARAMETER] - Dz Security Panel Password MD5 Encrypted 
idx_AlarmALERT             : '7',                                //[$$DOMOTICZ_PARAMETER] - Alarm Alert status - Dz "ALERTE Intrusion Maison" Device 
idx_AlarmARM               : '41'                                //[$$DOMOTICZ_PARAMETER] - Alarm Armed status - Dz "Maison sous Alarme" Device
};

module.exports   = mySecretKeys;