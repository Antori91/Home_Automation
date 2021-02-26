--[[
   @Antori91  http:--www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
   V0.10  - January 2021 - Initial release
]]

return {
    helpers = {
        MAIN_SERVER_HARDWARE       = 'SYNOLOGY',      -- Main Domoticz server Hardware= SYNOLOGY and RASPBERRY supported
        MAIN_SERVER_IP             = '192.168.Y.YY',  -- Main Domoticz server IP address
        MAIN_SERVER_SSH_PORT       =  YYYY,           -- Main Domoticz server SSH Port
        MAIN_SERVER_SSH_USER       = '---------',     -- Main Domoticz server SSH User
        MAIN_SERVER_ROOT_PWD       = '---------',     -- Main Domoticz server root Password
        BACKUP_SERVER_HARDWARE     = 'RASPBERRY',     -- Backup Domoticz server Hardware
        BACKUP_SERVER_IP           = '192.168.Y.YYY', -- Backup Domoticz server IP address
        INDOOR_ESP8266_DHT22       = '192.168.Y.YYY', -- Internal Temp sensor IP address
        OUTDOOR_ESP8266_DHT22      = '192.168.Y.YYY', -- OutdoorTemp sensor IP address
        TEMP_ESP8266_DHT22_PORT    =  YY,             -- Temp sensor HTTP port
        DZ_PORT                    =  YYYY,           -- Domoticz IP port         
        DZ_IN_TOPIC                = '---------',     -- Domoticz In mqtt topic
        DZ_OUT_TOPIC               = '---------',     -- Domoticz Out mqtt topic
        MQTT_PORT                  =  YYYY,           -- Mqtt IP port         
        IDX_INDOORTEMPHUM          = YY,                                 --[$$DOMOTICZ_PARAMETER] - Internal Temp sensor temp/humidity device IDX
        IDX_INDOORHEATIDX          = YY,                                 --[$$DOMOTICZ_PARAMETER] - Internal Temp sensor heat index device IDX
        IDX_OUTDOORTEMPHUM         = YY,                                 --[$$DOMOTICZ_PARAMETER] - External Temp sensor temp/humidity device IDX
        IDX_OUTDOORHEATIDX         = YY,                                 --[$$DOMOTICZ_PARAMETER] - External Temp sensor heat index device IDX
        IDX_ELECTARIFF             = YY,                                 --[$$DOMOTICZ_PARAMETER] - Current Electricity tariff used device IDX
        IDX_THERMOSTAT_SETPOINT    = YY,                                 --[$$DOMOTICZ_PARAMETER] - House Main Temp setpoint device IDX
        IDX_THERMALLOSS            = YY,                                 --[$$DOMOTICZ_PARAMETER] - House Thermal Loss device IDX
        IDX_COOLINGRATE            = YY,                                 --[$$DOMOTICZ_PARAMETER] - House Cooling Rate device IDX
        IDX_DJUTEMP                = YY,                                 --[$$DOMOTICZ_PARAMETER] - DegreeDays device IDX
        IDXCLUSTERFAILUREFLAG      = YY,                                 --[$$DOMOTICZ_PARAMETER] - General Home Automation Failure status (i.e. Hardware Server, Dz, Mqtt, ... ) - Dz "Panne Domotique" Device
        IDX_ALARMFAILUREFLAG       = YY,                                 --[$$DOMOTICZ_PARAMETER] - Alarm Failure status - Dz "Panne Alarme" Device
        IDX_TEMPFAILUREFLAG        = YY,                                 --[$$DOMOTICZ_PARAMETER] - Temp sensors Failure status - Dz "Panne Sondes Temperature" Device 
        IDX_HEATINGSELECTOR        = YY,                                 --[$$DOMOTICZ_PARAMETER] - Dz Global heating breaker (OFF/HORSGEL/ECO/CONFORT) Device
        IDX_UNACTIVEHEATERSDISPLAY = YY,                                 --[$$DOMOTICZ_PARAMETER] - Dz "Zones Chauffage Actives" Device
        IDX_ACTIVATEHEATERS        = YY,                                 --[$$DOMOTICZ_PARAMETER] - Dz "Horaires/Start Chauffage" Device
        IDX_UNACTIVATEHEATERS      = YY,                                 --[$$DOMOTICZ_PARAMETER] - Dz "Horaires/Stop Chauffage" Device
        VAR_UNACTIVEHEATERS        = 'YY',                               --[$$DOMOTICZ_PARAMETER] - Dz user variable# to store the heaters heating state command
        VARNAME_UNACTIVEHEATERS    = 'HeatersActive',                    --[$$DOMOTICZ_PARAMETER] - Dz user variable name to store the heaters heating state command
        PROTECTEDDEVICEPASSWORD    = 'YYYY',                             --[$$DOMOTICZ_PARAMETER] - Dz password for protected device
        IDX_SECPANEL               = 'YY',            -- Alarm Server Security Panel (MD5 signed message)
        DZ_IDX_SECPANEL            = YY,                                 --[$$DOMOTICZ_PARAMETER] - Dz Security Panel
        SECPANEL_SECCODE           = '--------------------------------', --[$$DOMOTICZ_PARAMETER] - Dz Security Panel Password MD5 Encrypted 
        IDX_ALARMALERT             = YY,                                 --[$$DOMOTICZ_PARAMETER] - Alarm Alert status - Dz "ALERTE Intrusion Maison" Device 
        IDX_ALARMARM               = YY,                                 --[$$DOMOTICZ_PARAMETER] - Alarm Armed status - Dz "Maison sous Alarme" Device
        IDX_FIREALARMSTATUS        = YY,                                 --[$$DOMOTICZ_PARAMETER] - Fire Alarm device IDX (Selector)
        IDX_FIREALARMTEMPHUM       = YY,                                 --[$$DOMOTICZ_PARAMETER] - Fire Alarm temp/humidity device IDX
        IDX_FIREALARMHEATIDX       = YY,                                 --[$$DOMOTICZ_PARAMETER] - Fire Alarm temp heat index device IDX
        SVR_ALARM_SID              = '-----------',   -- Alarm Server network SID
        DZ_ALARM_CID               = '-----------',   -- Dz Alarm network CID
        VTH_ALARM_CID              = '-----------',   -- VTH Alarm network CID
        VTO_ALARM_CID              = '-----------',   -- VTO Alarm network CID
        IDX_LINKYL1L2L3CURRENT     = YY,                                 --[$$DOMOTICZ_PARAMETER] - Linky current drawn for each phase
        IDX_LINKYPOWERLOAD         = YY,                                 --[$$DOMOTICZ_PARAMETER] - Linky Power Load
        IDX_LINKYUSAGEMETER        = YY,                                 --[$$DOMOTICZ_PARAMETER] - Linky usage meter (Base Tariff Option)
        IDX_UPSCHARGE              = YY,                                 --[$$DOMOTICZ_PARAMETER] - UPS battery charge
        IDX_UPSBACKUPTIME          = YY,                                 --[$$DOMOTICZ_PARAMETER] - UPS backup time available
        IDX_UPSLOAD                = YY                                  --[$$DOMOTICZ_PARAMETER] - UPS Power Load
    }
}
