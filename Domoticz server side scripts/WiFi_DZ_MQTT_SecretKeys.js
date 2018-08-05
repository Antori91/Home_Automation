// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// V0.2 - August 2018 

var mySecretKeys = {
MAIN_SERVER_IP             : '192.168.X.XX',  // Main Domoticz server
BACKUP_SERVER_IP           : '192.168.X.XXX', // Backup Domoticz server
DZ_PORT                    :  XXXX,           // The Domoticz IP port         
idxClusterFailureFlag      : 'XX',                              //[$$DOMOTICZ_PARAMETER] - General Home Automation Failure status (i.e. Hardware Server, Dz, Mqtt, ... ) - Dz "Panne Domotique" Device
idx_AlarmFailureFlag       : 'XX',                              //[$$DOMOTICZ_PARAMETER] - Alarm Failure status - Dz "Panne Alarme" Device
idx_TempFailureFlag        : 'XX',                              //[$$DOMOTICZ_PARAMETER] - Temp sensors Failure status - Dz "Panne Sondes Température" Device 
idx_HeatingSelector        : 'XX',                              //[$$DOMOTICZ_PARAMETER] - Dz Global heating breaker (OFF/HORSGEL/ECO/CONFORT) Device
idx_UnactiveHeatersDisplay : 'XX',                              //[$$DOMOTICZ_PARAMETER] - Dz "Zones Chauffage Actives" Device
idx_ActivateHeaters        : 'XX',                              //[$$DOMOTICZ_PARAMETER] - Dz "Horaires/Start Chauffage" Device
idx_UnactivateHeaters      : 'XX',                              //[$$DOMOTICZ_PARAMETER] - Dz "Horaires/Stop Chauffage" Device
Var_UnactiveHeaters        : 'X',                               //[$$DOMOTICZ_PARAMETER] - Dz user variable# to store the heaters heating state command
Varname_UnactiveHeaters    : 'HeatersActive',                   //[$$DOMOTICZ_PARAMETER] - Dz user variable name to store the heaters heating state command
ProtectedDevicePassword    : 'XXXX',                            //[$$DOMOTICZ_PARAMETER] - Dz password for protected device
SecPanel_Seccode           : '--------------------------------' //[$$DOMOTICZ_PARAMETER] - Dz Security Panel Password MD5 Encrypted 
};

module.exports   = mySecretKeys;