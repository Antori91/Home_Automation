// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// V0.40 - March 2021
// V0.31- May 2019
// V0.3 - March 2019
//        Fire Alarm Domoticz devices IDX added
// V0.2 - August 2018

// WiFi
#define WIFI_1                     "------------"
#define WIFI_2                     "------------"
#define WIFI_3                     "------------"
#define WIFI_4                     "------------"
#define WIFI_1_PWD                 "------------"
#define WIFI_2_PWD                 "------------"
#define WIFI_3_PWD                 "------------"
#define WIFI_4_PWD                 "------------"
#ifdef FIXED_IP_USED
#include <ESP8266WiFi.h>
IPAddress gateway(192,168,X,XXX);
IPAddress subnet(255,255,255,0);
#endif

// MQTT
#define MQTT_SERVER_IP             "192.168.X.XXX" 
#define MQTT_PORT                  YYYY           // Mqtt IP port

// OTA
#define OTA_PWD                    "------------"

// Http Json TEMP sensor
#define OUTDOOR_TEMP_IP            IPAddress(192,168,X,XXX)
#define INDOOR_TEMP_IP             IPAddress(192,168,X,XXX) 
#define NEW_TEMP_IP                IPAddress(192,168,X,XXX)           // Reserved - Future additional Http Json Temp sensor
#define OUTDOOR_TEMP_ID            "------------"
#define INDOOR_TEMP_ID             "------------"
#define INDOOR_RDC_TEMP_ID         "------------"
#define NEW_TEMP_ID                "------------"
#define TEMP_ESP8266_DHT22_PORT    YYYY                               // Temp sensor HTTP port 

// Domoticz
#define DZ_PORT                    YYYY                               //[$$DOMOTICZ_PARAMETER] - Domoticz IP port         
#define DZ_IN_TOPIC                "------------"                     //[$$DOMOTICZ_PARAMETER] - Domoticz In mqtt topic
#define DZ_OUT_TOPIC               "------------"                     //[$$DOMOTICZ_PARAMETER] - Domoticz Out mqtt topic
#define IDX_INDOORTEMPHUM          "XX"                               //[$$DOMOTICZ_PARAMETER] - Internal Temp sensor temp/humidity device IDX
#define IDX_INDOORHEATIDX          "XX"                               //[$$DOMOTICZ_PARAMETER] - Internal Temp sensor heat index device IDX
#define IDX_OUTDOORTEMPHUM         "XX"                               //[$$DOMOTICZ_PARAMETER] - External Temp sensor temp/humidity device IDX
#define IDX_OUTDOORHEATIDX         "XX"                               //[$$DOMOTICZ_PARAMETER] - External Temp sensor heat index device IDX
#define IDX_ELECTARIFF             "XX"                               //[$$DOMOTICZ_PARAMETER] - Current Electricity tariff used device IDX
#define IDX_THERMOSTAT_SETPOINT    "XX"                               //[$$DOMOTICZ_PARAMETER] - House Main Temp setpoint device IDX
#define IDX_THERMALLOSS            "XX"                               //[$$DOMOTICZ_PARAMETER] - House Thermal Loss device IDX
#define IDX_COOLINGRATE            "XX"                               //[$$DOMOTICZ_PARAMETER] - House Cooling Rate device IDX
#define IDX_DJUTEMP                "XX"                               //[$$DOMOTICZ_PARAMETER] - DegreeDays device IDX
#define IDXCLUSTERFAILUREFLAG      "XX"                               //[$$DOMOTICZ_PARAMETER] - General Home Automation Failure status (i.e. Hardware Server, Dz, Mqtt, ... ) - Dz "Panne Domotique" Device
#define IDX_ALARMFAILUREFLAG       "XX"                               //[$$DOMOTICZ_PARAMETER] - Alarm Failure status - Dz "Panne Alarme" Device
#define IDX_TEMPFAILUREFLAG        "XX"                               //[$$DOMOTICZ_PARAMETER] - Temp sensors Failure status - Dz "Panne Sondes Temperature" Device 
#define IDX_HEATINGSELECTOR        "XX"                               //[$$DOMOTICZ_PARAMETER] - Dz Global heating breaker (OFF/HORSGEL/ECO/CONFORT) Device
#define IDX_UNACTIVEHEATERSDISPLAY "XX"                               //[$$DOMOTICZ_PARAMETER] - Dz "Zones Chauffage Actives" Device
#define IDX_ACTIVATEHEATERS        "XX"                               //[$$DOMOTICZ_PARAMETER] - Dz "Horaires/Start Chauffage" Device
#define IDX_UNACTIVATEHEATERS      "XX"                               //[$$DOMOTICZ_PARAMETER] - Dz "Horaires/Stop Chauffage" Device
#define VAR_UNACTIVEHEATERS        "XX"                               //[$$DOMOTICZ_PARAMETER] - Dz user variable# to store the heaters heating state command
#define VARNAME_UNACTIVEHEATERS    "HeatersActive"                    //[$$DOMOTICZ_PARAMETER] - Dz user variable name to store the heaters heating state command
#define DZ_DEVICE_PASSWORD         "XXXX"                             //[$$DOMOTICZ_PARAMETER] - Dz password for protected device
//#define IDX_SECPANEL             "XX"                               // Alarm Server Security Panel (MD5 signed message)
#define DZ_IDX_SECPANEL            "XX"                               //[$$DOMOTICZ_PARAMETER] - Dz Security Panel
#define SECPANEL_SECCODE           "--------------------------------" //[$$DOMOTICZ_PARAMETER] - Dz Security Panel Password MD5 Encrypted 
#define IDX_ALARMALERT             "XX"                               //[$$DOMOTICZ_PARAMETER] - Alarm Alert status - Dz "ALERTE Intrusion Maison" Device 
#define IDX_ALARMARM               "XX"                               //[$$DOMOTICZ_PARAMETER] - Alarm Armed status - Dz "Maison sous Alarme" Device
#define FIREALARM_SWITCH_IDX       "XX"                               //[$$DOMOTICZ_PARAMETER] - Fire Alarm device IDX (Selector)
#define FIREALARM_TEMP_IDX         "XX"                               //[$$DOMOTICZ_PARAMETER] - Fire Alarm temp/humidity device IDX
#define FIREALARM_HEAT_IDX         "XX"                               //[$$DOMOTICZ_PARAMETER] - Fire Alarm temp heat index device IDX
#define FIREALARM_SMOKE_IDX        "XX"                               //[$$DOMOTICZ_PARAMETER] - Fire Alarm smoke device IDX
#define SVR_ALARM_SID              "------------"                     // Alarm Server network SID
#define DZ_ALARM_CID               "------------"                     // Dz Alarm network CID
#define VTH_ALARM_CID              "------------"                     // VTH Alarm network CID
#define VTO_ALARM_CID              "------------"                     // VTO Alarm network CID
#define IDX_LINKYL1L2L3CURRENT     "XX"                               //[$$DOMOTICZ_PARAMETER] - Linky current drawn for each phase
#define IDX_LINKYPOWERLOAD         "XX"                               //[$$DOMOTICZ_PARAMETER] - Linky Power Load
#define IDX_LINKYUSAGEMETER        "XX"                               //[$$DOMOTICZ_PARAMETER] - Linky usage meter (Base Tariff Option)
#define IDX_UPSCHARGE              "XX"                               //[$$DOMOTICZ_PARAMETER] - UPS battery charge
#define IDX_UPSBACKUPTIME          "XX"                               //[$$DOMOTICZ_PARAMETER] - UPS backup time available
#define IDX_UPSLOAD                "XX"                               //[$$DOMOTICZ_PARAMETER] - UPS Power Load


