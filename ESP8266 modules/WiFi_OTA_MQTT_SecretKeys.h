// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// V0.31- May 2019
// V0.3 - March 2019
//        Fire Alarm Domoticz devices IDX added
// V0.2 - August 2018

#define WIFI_1             "YOUR_SSID_1"
#define WIFI_2             "YOUR_SSID_2"
#define WIFI_3             "YOUR_SSID_3"
#define WIFI_4             "YOUR_SSID_4"
#define WIFI_1_PWD         "Your_Password_For_SSID_1"
#define WIFI_2_PWD         "Your_Password_For_SSID_2"
#define WIFI_3_PWD         "Your_Password_For_SSID_3"
#define WIFI_4_PWD         "Your_Password_For_SSID_4"
#define MQTT_SERVER_IP     "192.168.XX.XX"
#define OTA_PWD            "Your_Password_For_OTA"
#define OUTDOOR_TEMP_IP    IPAddress(192,168,X,XXX)
#define INDOOR_TEMP_IP     IPAddress(192,168,X,XXX) 
#define NEW_TEMP_IP        IPAddress(192,168,X,XXX)
#define OUTDOOR_TEMP_ID    "YOUR SENSOR ID1"
#define INDOOR_TEMP_ID     "YOUR SENSOR ID2"
#define INDOOR_RDC_TEMP_ID "YOUR SENSOR ID3"
#define NEW_TEMP_ID        "YOUR SENSOR ID4"
#ifdef FIXED_IP_USED
#include <ESP8266WiFi.h>
IPAddress gateway(192,168,X,X);
IPAddress subnet(255,255,255,0);
#endif
#define DZ_DEVICE_PASSWORD  "----"   // Domoticz Password for protected device
#define FIREALARM_SWITCH_IDX  "XX"   // Domoticz Fire Alarm device IDX (Selector)
#define FIREALARM_TEMP_IDX    "66"   // Domoticz Fire Alarm temp/humidity device IDX
#define FIREALARM_HEAT_IDX    "67"   // Domoticz Fire Alarm temp heat index device IDX
