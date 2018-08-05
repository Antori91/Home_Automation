#define WIFI_1            "YOUR_SSID_1"
#define WIFI_2            "YOUR_SSID_2"
#define WIFI_3            "YOUR_SSID_3"
#define WIFI_4            "YOUR_SSID_4"
#define WIFI_1_PWD        "Your_Password_For_SSID_1"
#define WIFI_2_PWD        "Your_Password_For_SSID_2"
#define WIFI_3_PWD        "Your_Password_For_SSID_3"
#define WIFI_4_PWD        "Your_Password_For_SSID_4"
#define MQTT_SERVER_IP    "192.168.XX.XX"
#define OTA_PWD           "Your_Password_For_OTA"
#define OUTDOOR_TEMP_IP   IPAddress(192,168,X,XXX)
#define INDOOR_TEMP_IP    IPAddress(192,168,X,XXX) 
#define NEW_TEMP_IP       IPAddress(192,168,X,XXX)
#define OUTDOOR_TEMP_ID   "YOUR SENSOR ID1"
#define INDOOR_TEMP_ID    "YOUR SENSOR ID2"
#define NEW_TEMP_ID       "YOUR SENSOR ID3"
#ifdef FIXED_IP_USED
#include <ESP8266WiFi.h>
IPAddress gateway(192,168,X,X);
IPAddress subnet(255,255,255,0);
#endif

