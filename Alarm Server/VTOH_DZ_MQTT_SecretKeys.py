# @Antori91  http:#www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
# V0.1 - February 2020
# Initial version

mySecretKeys = {
'MAIN_SERVER_HARDWARE'       : 'SYNOLOGY',        # Main Domoticz server Hardware: SYNOLOGY and RASPBERRY supported
'MAIN_SERVER_IP'             : '192.168.X.XX',    # Main Domoticz server IP address
'MAIN_SERVER_SSH_PORT'       :  XXX,              # Main Domoticz server SSH Port
'MAIN_SERVER_SSH_USER'       : '-------',         # Main Domoticz server SSH User
'MAIN_SERVER_ROOT_PWD'       : '-----------',     # Main Domoticz server root Password
'BACKUP_SERVER_HARDWARE'     : 'RASPBERRY',       # Backup Domoticz server Hardware
'BACKUP_SERVER_IP'           : '192.168.X.XXX',   # Backup Domoticz server IP address
'DZ_PORT'                    :  XXXX,             # Domoticz IP port  
'DZ_IN_TOPIC'                :  '-----------',    # Domoticz In mqtt topic
'DZ_OUT_TOPIC'               :  '------------',   # Domoticz Out mqtt topic
'MQTT_PORT'                  :  XXXX,             # Mqtt IP port         
'VTO_SERVER_IP'              : '192.168.X.XXX',   # VTO IP address
'VTO_PORT'                   :  XXXX,             # VTO DHIP port
'VTO_PROTOCOL'               :  '----',           # Dahua protocol choosen to communicate with the VTO
'VTO_CREDENTIALS'            : '-----:--------',  # VTO admin/password  
'VTO_SERIAL_NUMBER'          : '',                # VTO serial number 
'VTH_SERVER_IP'              : '192.168.X.XXX',   # VTH IP address
'VTH_PORT'                   :  XXXX,             # VTH DHIP port
'VTH_PROTOCOL'               :  '----',           # Dahua protocol choosen to communicate with the VTH
'VTH_CREDENTIALS'            : '-----:------',    # VTH activation admin/password 
'VTH_SERIAL_NUMBER'          : '---------------', # VTH serial number
'idxClusterFailureFlag'      : XX,                                 #[$$DOMOTICZ_PARAMETER] - General Home Automation Failure status (i.e. Hardware Server, Dz, Mqtt, ... ) - Dz 'Panne Domotique' Device
'idx_AlarmFailureFlag'       : XX,                                 #[$$DOMOTICZ_PARAMETER] - Alarm Failure status - Dz 'Panne Alarme' Device
'idx_TempFailureFlag'        : XX,                                 #[$$DOMOTICZ_PARAMETER] - Temp sensors Failure status - Dz 'Panne Sondes Temperature' Device 
'idx_HeatingSelector'        : XX,                                 #[$$DOMOTICZ_PARAMETER] - Dz Global heating breaker (OFF/HORSGEL/ECO/CONFORT) Device
'idx_UnactiveHeatersDisplay' : XX,                                 #[$$DOMOTICZ_PARAMETER] - Dz 'Zones Chauffage Actives' Device
'idx_ActivateHeaters'        : XX,                                 #[$$DOMOTICZ_PARAMETER] - Dz 'Horaires/Start Chauffage' Device
'idx_UnactivateHeaters'      : XX,                                 #[$$DOMOTICZ_PARAMETER] - Dz 'Horaires/Stop Chauffage' Device
'Var_UnactiveHeaters'        : 'X',                                #[$$DOMOTICZ_PARAMETER] - Dz user variable# to store the heaters heating state command
'Varname_UnactiveHeaters'    : '-------------',                    #[$$DOMOTICZ_PARAMETER] - Dz user variable name to store the heaters heating state command
'ProtectedDevicePassword'    : '----',                             #[$$DOMOTICZ_PARAMETER] - Dz password for protected device
'idx_SecPanel'               : 'X',                                #                       - Alarm Server Security Panel (MD5 signed message)
'DZ_idx_SecPanel'            :  X,                                 #[$$DOMOTICZ_PARAMETER] - Dz Security Panel
'SecPanel_Seccode'           : '--------------------------------', #[$$DOMOTICZ_PARAMETER] - Dz Security Panel Password MD5 Encrypted 
'idx_AlarmALERT'             : XX,                                 #[$$DOMOTICZ_PARAMETER] - Alarm Alert status - Dz 'ALERTE Intrusion Maison' Device 
'idx_AlarmARM'               : XX,                                 #[$$DOMOTICZ_PARAMETER] - Alarm Armed status - Dz 'Maison sous Alarme' Device
'idx_FireALarmStatus'        : XX,                                 #[$$DOMOTICZ_PARAMETER] - Fire Alarm device IDX (Selector)
'idx_FireAlarmTempHum'       : XX,                                 #[$$DOMOTICZ_PARAMETER] - Fire Alarm temp/humidity device IDX
'idx_FireAlarmHeatidx'       : XX,                                 #[$$DOMOTICZ_PARAMETER] - Fire Alarm temp heat index device IDX
'SVR_ALARM_SID'              : '------------',  # Alarm Server network SID
'DZ_ALARM_CID'               : '------------',  # Dz Alarm network CID
'VTH_ALARM_CID'              : '------------',  # VTH Alarm network CID
'VTO_ALARM_CID'              : '------------'   # VTO Alarm network CID
}