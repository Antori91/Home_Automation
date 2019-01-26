#Note about PubSub library
iot_ESP8266_ACS712 and iot_ESP8266_GM43 modules use the PubSub library to communicate with Domoticz using MQTT.  
To get it working, you have to modify the PubSubClient.h file located in ...\Arduino\libraries\PubSubClient\src.  
The "#define MQTT_MAX_PACKET_SIZE 128" line must be changed to #define "MQTT_MAX_PACKET_SIZE 1024"  
**This is an MANDATORY requirement.**