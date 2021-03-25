// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// V0.81 - March 2021
// Fire Alarm Smoke IDX added
// V0.8 - January 2021
// Various Temp devices added
// V0.7/0.71 - December 2020
// UPS and Linky meter added
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
INDOOR_ESP8266_DHT22       : '192.168.Y.YYY', // Internal Temp sensor IP address
OUTDOOR_ESP8266_DHT22      : '192.168.Y.YYY', // OutdoorTemp sensor IP address
TEMP_ESP8266_DHT22_PORT    :  YYYY,           // Temp sensor HTTP port
DZ_PORT                    :  YYYY,           // Domoticz IP port         
DZ_IN_TOPIC                : '-----------',   // Domoticz In mqtt topic
DZ_OUT_TOPIC               : '-----------',   // Domoticz Out mqtt topic
MQTT_PORT                  :  YYYY,           // Mqtt IP port   
idx_IndoorTempHum          : 'YY',                               //[$$DOMOTICZ_PARAMETER] - Internal Temp sensor temp/humidity device IDX
idx_IndoorHeatidx          : 'YY',                               //[$$DOMOTICZ_PARAMETER] - Internal Temp sensor heat index device IDX
idx_OutdoorTempHum         : 'YY',                               //[$$DOMOTICZ_PARAMETER] - External Temp sensor temp/humidity device IDX
idx_OutdoorHeatidx         : 'YY',                               //[$$DOMOTICZ_PARAMETER] - External Temp sensor heat index device IDX
idx_ElecTariff             : 'YY',                               //[$$DOMOTICZ_PARAMETER] - Current Electricity tariff used device IDX
idx_Thermostat_setPoint    : 'YY',                               //[$$DOMOTICZ_PARAMETER] - House Main Temp setpoint device IDX
idx_ThermalLoss            : 'YY',                               //[$$DOMOTICZ_PARAMETER] - House Thermal Loss device IDX
idx_CoolingRate            : 'YY',                               //[$$DOMOTICZ_PARAMETER] - House Cooling Rate device IDX
idx_DJUTemp                : 'YY',                               //[$$DOMOTICZ_PARAMETER] - DegreeDays device IDX      
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
idx_FireAlarmSmokeidx      : YY,                                 //[$$DOMOTICZ_PARAMETER] - Fire Alarm smoke device IDX
SVR_ALARM_SID              : '-----------',  // Alarm Server network SID
DZ_ALARM_CID               : '-----------',  // Dz Alarm network CID
VTH_ALARM_CID              : '-----------',  // VTH Alarm network CID
VTO_ALARM_CID              : '-----------',  // VTO Alarm network CID
idx_LinkyL1L2L3current     : YY,                                 //[$$DOMOTICZ_PARAMETER] - Linky current drawn for each phase
idx_LinkyPowerLoad         : YY,                                 //[$$DOMOTICZ_PARAMETER] - Linky Power Load
idx_LinkyUsageMeter        : YY,                                 //[$$DOMOTICZ_PARAMETER] - Linky usage meter (Base Tariff Option)
idx_UPSCharge              : YY,                                 //[$$DOMOTICZ_PARAMETER] - UPS battery charge
idx_UPSBackupTime          : YY,                                 //[$$DOMOTICZ_PARAMETER] - UPS backup time available
idx_UPSLoad                : YY                                  //[$$DOMOTICZ_PARAMETER] - UPS Power Load
};

module.exports   = mySecretKeys;