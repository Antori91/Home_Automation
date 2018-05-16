// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// ***** Lighting management 
//    Interfaces two GM43 devices with ElectroDragon IoT WiFi SPST/SPDT Relay board 
//    Lighting connected to relay *NO* output on SPDT (don't switch ON/OFF lamps during boot time)
// V0.54 May 2018

#include "WiFi_OTA_MQTT_SecretKeys.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// EDragon/ESP8266 board parameters
boolean ColdBoot                  = true;
#define FILTER                       3 // N+1 same readings every DELAY_FILTER ms must be read to validate GPIO reading ! 
#define DELAY_FILTER                25 // Delay in ms between two readings
#define Relay1                      12 // Digital Pin 12
#define Relay2                      13 // Digital Pin 13
#define GPIO4                        4 // GPIO-5 on SPDT connector
#define GPIO5                        5 // GPIO-4 on SPDT connector
 
// WiFi parameters
#define   NUMBER_OF_AP 4
byte      WiFi_AP    = 2; // The WiFi Access Point we are connected to 
const     char *ssid[NUMBER_OF_AP]      = { WIFI_1,     WIFI_2,     WIFI_3,     WIFI_4 };
const     char *password[NUMBER_OF_AP]  = { WIFI_1_PWD, WIFI_2_PWD, WIFI_3_PWD, WIFI_4_PWD };
boolean   WiFiWasRenew = false;
#define   WiFiRetryDelay 2000 // Delay in ms between each WiFi connection retry

// MQTT parameters
byte          willQoS            = 0;
char          willMessage[MQTT_MAX_PACKET_SIZE+1];
boolean       willRetain         = false;
const char*   mqtt_server        = MQTT_SERVER_IP;
const char*   topic_Domoticz_IN  = "domoticz/in"; 
const char*   topic_Domoticz_OUT = "domoticz/out";         
char          msgToPublish[MQTT_MAX_PACKET_SIZE+1]; 
WiFiClient    espClient;
PubSubClient  client(espClient);
#define       MQTTRetryDelay       2000 // Delay in ms between each MQTT connection retry
#define       MQTTMaxRetriesDelay  10  // if we didn't success to reconnect in that delay in mn, then restart the ESP
const int     MQTTMaxRetries     = MQTTMaxRetriesDelay * ( 60000 / ( 4000 + MQTTRetryDelay + FILTER*2*DELAY_FILTER ) ); // 4s is around the connection try duration
int           MQTTRetriesCounter = MQTTMaxRetries;    

// Lighting parameters
String      LIGHTING[2]         = { "ENTREE", "MEZZANINE" }; //$$ 
String      LIGHT_SWITCH_IDX[2] = { "50", "51" };            // Corresponding DomoticZ switchs
String      LIGHT_ACTIVE[2]     = { "Off", "Off" };          // Lighting state (On or Off)    
boolean     LIGHT_CHANGED[2]    = { true, true };            // Flag to know if we must update DomoticZ because people have changed Lighting state using home pushbuttons. Initialize to *true* to reset DomoticZ switchs at boot.
int         cstate_GM43[2];                                  // GM43 current device status      
int         pstate_GM43[2];                                  // GM43 previous device status           
const char* LIGHT_MQTT_ID       = "TWILIGHT";
                 
void eventWiFi(WiFiEvent_t event) {
    Serial.println("== WiFi Event Received ==");
    switch(event) {
        case WIFI_EVENT_STAMODE_CONNECTED:
          Serial.print("[WiFi STA] "); /*Serial.print(event);*/ Serial.println(" Connected");      
          break;
        case WIFI_EVENT_STAMODE_DISCONNECTED:
          Serial.print("[WiFi STA] "); /*Serial.print(event);*/ Serial.print(" Disconnected - Status :"); Serial.print(WiFi.status()); Serial.print(" "); Serial.println(connectionStatus( WiFi.status()));
          break;
        case WIFI_EVENT_STAMODE_AUTHMODE_CHANGE:
          Serial.print("[WiFi STA] "); /*Serial.print(event);*/ Serial.println(" AuthMode Change");
          break;
        case WIFI_EVENT_STAMODE_GOT_IP:
          Serial.print("[WiFi STA] "); /*Serial.print(event);*/ Serial.println(" Got IP");
          Serial.printf(" == SUCCESS - Connected to WiFi AP %s ==\n", WiFi.SSID().c_str());
          WiFiWasRenew = true;
          break;
        case WIFI_EVENT_STAMODE_DHCP_TIMEOUT:
          Serial.print("[WiFi STA] "); /*Serial.print(event);*/ Serial.println(" DHCP Timeout");
          break;
        case WIFI_EVENT_SOFTAPMODE_STACONNECTED:
          Serial.print("[WiFi AP] "); /*Serial.print(event);*/ Serial.println(" Client Connected");
          break;
        case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED:
          Serial.print("[WiFi AP] "); /*Serial.print(event);*/ Serial.println(" Client Disconnected");
          break;
        case WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED:
          Serial.print("[WiFi AP] "); /*Serial.print(event);*/ Serial.println(" Probe Request Received");
          break;
    }  // switch(event) {
} // void eventWiFi(WiFiEvent_t event) {

String connectionStatus( int which ) {
    switch ( which ) {
        case WL_CONNECTED:
        return "Connected";
        break;
        case WL_NO_SSID_AVAIL:
        return "AP not available";
        break;
        case WL_CONNECT_FAILED:
        return "Connect Failed";
        break;
        case WL_IDLE_STATUS:
        return "Idle status";
        break;
        case WL_DISCONNECTED:
        return "Disconnected";
        break;
        default:
        return "Unknown";
        break;
    }
} // String connectionStatus( int which ) {

void setup() {   // ****************
      
   Serial.begin(115200);
   Serial.println("iot_EDRAGON_GM43_Lighting Booting - Firmware Version : 0.54");
   
   pinMode(Relay1,OUTPUT);   
   pinMode(Relay2,OUTPUT);  
   digitalWrite(Relay1, LOW);      // Lighting switch OFF 
   digitalWrite(Relay2, LOW);
   pinMode(GPIO4,INPUT);   
   pinMode(GPIO5,INPUT); 
   pstate_GM43[0] = digitalReadF(GPIO4,false); cstate_GM43[0] = pstate_GM43[0];
   pstate_GM43[1] = digitalReadF(GPIO5,false); cstate_GM43[1] = pstate_GM43[1];
   
   // connect to WiFi Access Point
   WiFi.onEvent(eventWiFi);
   WiFi.mode(WIFI_STA);
   Serial.printf( "Trying to connect to preferred WiFi AP %s\n", ssid[WiFi_AP] );
   WiFi.begin(ssid[WiFi_AP], password[WiFi_AP]);
   while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      PushbuttonsPressed(false);
      if( ++WiFi_AP == NUMBER_OF_AP ) WiFi_AP = 0;
      delay(WiFiRetryDelay);
      Serial.printf( "Trying to connect to WiFi AP %s\n", ssid[WiFi_AP] );
      WiFi.begin(ssid[WiFi_AP], password[WiFi_AP]);
   } // while (WiFi.waitForConnectResult() != WL_CONNECTED) {

   // if ( MDNS.begin ( "esp8266" ) ) Serial.println ( "MDNS responder started" );
   
   // Port defaults to 8266
   // ArduinoOTA.setPort(8266);
   // Set OTA Hostname
   ArduinoOTA.setHostname(LIGHT_MQTT_ID);  
   // Set OTA authentication password
   ArduinoOTA.setPassword((const char *)OTA_PWD);
   ArduinoOTA.onStart([]() {
    Serial.println("Start");
   });
   ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
   });
   ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
   });
   ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)         Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)     Serial.println("End Failed");
   });
   ArduinoOTA.begin();

   // MQTT 
   client.setServer(mqtt_server, 1883);
   client.setCallback(callback);

   // say we are now ready and give configuration items
   Serial.println( "Ready" );
   Serial.print  ( "Connected to " ); Serial.printf ( "%s\n", ssid[WiFi_AP] );
   Serial.print  ( "IP address: " );  Serial.println( WiFi.localIP() );  
   Serial.print  ( "Lighting " ); Serial.print(LIGHTING[0]); Serial.print(" set to : "); Serial.print(LIGHT_ACTIVE[0]); Serial.print(" - its GM43-GPIO4 is : "); Serial.println(cstate_GM43[0]);
   Serial.print  ( "Lighting " ); Serial.print(LIGHTING[1]); Serial.print(" set to : "); Serial.print(LIGHT_ACTIVE[1]); Serial.print(" - its GM43-GPIO5 is : "); Serial.println(cstate_GM43[1]); 
 
} // void setup(  ****************


void callback(char* topic, byte* payload, unsigned int length) {   // ****************
   
   DynamicJsonBuffer jsonBuffer( MQTT_MAX_PACKET_SIZE );
   String messageReceived="";
   
   // Affiche le topic entrant - display incoming Topic
   Serial.print("\nEntering MQTT Callback. Message arrived regarding topic [");
   Serial.print(topic);
   Serial.println("]");

   // decode payload message
   for (int i = 0; i < length; i++) {
   messageReceived+=((char)payload[i]); 
   }
   // display incoming message
   // Serial.println(messageReceived);
     
   // if domoticz message
   if ( strcmp(topic, topic_Domoticz_OUT) == 0 ) {
        Serial.println("MQTT Callback. Parsing Domoticz Message arrived...");
        JsonObject& root = jsonBuffer.parseObject(messageReceived);
        if (!root.success()) {
           Serial.println("parsing Domoticz/out JSON Received Message failed");
           return;
        }

        const char* idxChar = root["idx"];
        String idx = String( idxChar);
        Serial.print("MQTT Callback. This message is about Domoticz Device Idx=");Serial.println(idx);
        
        if ( idx == LIGHT_SWITCH_IDX[0] ) {      
           const char* cmde = root["nvalue"];
           Serial.print("MQTT Callback. Light Command received="); Serial.println(cmde);
           if( strcmp(cmde, "0") == 0 ) {  // 0 means we have to switch OFF the lights
                if( LIGHT_ACTIVE[0] == "On" ) { Serial.print("MQTT Callback. Turning Off Lighting "); Serial.println(LIGHTING[0]); digitalWrite(Relay1, LOW); delay(2000); LIGHT_ACTIVE[0] = "Off"; } 
           } else if( LIGHT_ACTIVE[0] == "Off" ) { Serial.print("MQTT Callback. Turning On Lighting "); Serial.println(LIGHTING[0]); digitalWrite(Relay1, HIGH); delay(2000); LIGHT_ACTIVE[0] = "On"; }           
           Serial.print  ( "Lighting " ); Serial.print(LIGHTING[0]); Serial.print(" set from Domoticz to : "); Serial.print(LIGHT_ACTIVE[0]); Serial.print(" - its GM43-GPIO4 is : "); Serial.println(cstate_GM43[0]);
        }  // if ( idx == LIGHT_SWITCH_IDX[0] ) {

        if ( idx == LIGHT_SWITCH_IDX[1] ) {      
           const char* cmde = root["nvalue"];
           Serial.print("MQTT Callback. Light Command received="); Serial.println(cmde);
           if( strcmp(cmde, "0") == 0 ) { 
                if( LIGHT_ACTIVE[1] == "On" ) { Serial.print("MQTT Callback. Turning Off Lighting "); Serial.println(LIGHTING[1]); digitalWrite(Relay2, LOW); delay(2000); LIGHT_ACTIVE[1] = "Off"; } 
           } else if( LIGHT_ACTIVE[1] == "Off" ) { Serial.print("MQTT Callback. Turning On Lighting "); Serial.println(LIGHTING[1]); digitalWrite(Relay2, HIGH); delay(2000); LIGHT_ACTIVE[1] = "On"; }    
            Serial.print  ( "Lighting " ); Serial.print(LIGHTING[1]); Serial.print(" set from Domoticz to : "); Serial.print(LIGHT_ACTIVE[1]); Serial.print(" - its GM43-GPIO5 is : "); Serial.println(cstate_GM43[1]); 
        }  // if ( idx == LIGHT_SWITCH_IDX[1] ) {
                   
   } // if domoticz message
  
   delay(15); 
   Serial.println("Leaving MQTT Callback");
} // void callback(char* to   **************** 


void reconnect() {   // ****************
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String string;
    
    // Attempt to connect
    string = "{\"command\" : \"addlogmessage\", \"message\" : \"Lighting went Offline - IP : " +  WiFi.localIP().toString() + "\"}";
    string.toCharArray( willMessage, MQTT_MAX_PACKET_SIZE);
    // if ( client.connect(buf) ) {
    if ( client.connect( LIGHT_MQTT_ID, topic_Domoticz_IN, willQoS, willRetain, willMessage ) ) {
        Serial.println("connected");
        
        // suscribe to MQTT topics
        Serial.print("Subscribe to domoticz/out topic. Status=");
        if ( client.subscribe(topic_Domoticz_OUT, 0) ) Serial.println("OK"); else Serial.println("KO"); 
   
        // Wait ten seconds to try to be sure DomoticZ available after a DomoticZ server reboot (i.e. means avoid MQTT available but not yet DomoticZ)
        // delay(10000);
        
        // Say now "Me the lighting, I'm here" 
        if( ColdBoot )
             string = "{\"command\" : \"addlogmessage\", \"message\" : \"iot_EDRAGON_GM43_Lighting Online - COLD BOOT Reason : " + ESP.getResetReason() + " - Firmware Version : 0.54 - SSID/IP/MAC : " + WiFi.SSID() + "/" + WiFi.localIP().toString() + "/" + WiFi.macAddress().c_str() + "\"}";
        else if ( WiFiWasRenew )
                  string = "{\"command\" : \"addlogmessage\", \"message\" : \"iot_EDRAGON_GM43_Lighting Online - WIFI CONNECTION RENEWED - SSID/IP/MAC : " + WiFi.SSID() + "/" + WiFi.localIP().toString() + "/" + WiFi.macAddress().c_str() + "\"}";   
             else string = "{\"command\" : \"addlogmessage\", \"message\" : \"iot_EDRAGON_GM43_Lighting Online - MQTT CONNECTION RENEWED - SSID/IP/MAC : " + WiFi.SSID() + "/" + WiFi.localIP().toString() + "/" + WiFi.macAddress().c_str() + "\"}";   
        string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
        Serial.print(msgToPublish);
        Serial.print("Published to domoticz/in. Status=");
        if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK"); else Serial.println("KO"); 
        ColdBoot = false; WiFiWasRenew = false;                         
    } else {
        Serial.print("MQTT connection failed, rc=");
        Serial.print(client.state());
        Serial.print(", MQTTRetriesCounter="); 
        Serial.print( MQTTRetriesCounter );
        Serial.print(" - Try again in ");
        Serial.print( (int)(MQTTRetryDelay/1000) );
        Serial.println(" seconds");
        PushbuttonsPressed(false);
        if( MQTTRetriesCounter-- == 0 ) ESP.restart();
        // Wait n seconds before retrying
        delay(MQTTRetryDelay);
    }  // if (client.connect
    
  } // while (!client.connected()) {
} // void reconnect() { **************** 

int digitalReadF(int GPIO, boolean DoMqttWork) {
  int i;
  int Reading = digitalRead(GPIO);
  if( Reading != 0 && Reading != 1 ) { 
      Serial.print("\n==== Error reading GPIO : "); 
      Serial.println(GPIO);
  } 
  int NextReading;
  for( i=1; i <= FILTER ; i++ ) {
    if( DoMqttWork ) client.loop();
    delay( DELAY_FILTER );
    NextReading = digitalRead(GPIO);
    if( NextReading != 0 && NextReading != 1 ) { 
      Serial.print("\n==== Error reading GPIO : "); 
      Serial.println(GPIO);
    } 
    if( NextReading != Reading ) { 
       i = 1;
       Reading = NextReading; 
       Serial.print("\n==== Restarting reading GPIO : "); Serial.println(GPIO);
    }
  }
  return( Reading );
} // int digitalReadF(int GPIO, boolean DoMqttWork) {

void PushbuttonsPressed(boolean DoMqttWork) {  // ****************   
 
  cstate_GM43[0] = digitalReadF(GPIO4,DoMqttWork);  
  if( cstate_GM43[0] != 0 && cstate_GM43[0] != 1 ) { 
      Serial.print("\n==== Error cstate_GM43[0] : "); 
      Serial.println(GPIO4);
  } 
  if( cstate_GM43[0] != pstate_GM43[0] )  {
      Serial.print( "\nLighting is going to changed. " ); Serial.print(LIGHTING[0]); Serial.print(" was : "); Serial.print(LIGHT_ACTIVE[0]); Serial.print(" - its GM43-GPIO4 is now : "); Serial.println(cstate_GM43[0]);
      pstate_GM43[0] = cstate_GM43[0];
      LIGHT_CHANGED[0]=true;
      if( LIGHT_ACTIVE[0] == "Off") { 
          digitalWrite(Relay1, HIGH); delay(2000); LIGHT_ACTIVE[0] = "On";
      } else { 
          digitalWrite(Relay1, LOW);  delay(2000); LIGHT_ACTIVE[0] = "Off";
      } // if( LIGHT_ACTIVE[0] == "Off") { 
      Serial.print( "Lighting has changed. " ); Serial.print(LIGHTING[0]); Serial.print(" set from Pushbuttons to : "); Serial.print(LIGHT_ACTIVE[0]); Serial.print(" - its GM43-GPIO4 is : "); Serial.println(cstate_GM43[0]);
  } // if( cstate_GM43[0] != pstate_GM43[0] )  {

  cstate_GM43[1] = digitalReadF(GPIO5,DoMqttWork);  
  if( cstate_GM43[1] != 0 && cstate_GM43[1] != 1 ) { 
      Serial.print("\n==== Error cstate_GM43[1] : "); 
      Serial.println(GPIO5);
  } 
  if( cstate_GM43[1] != pstate_GM43[1] )  {
      Serial.print( "\nLighting is going to changed. " ); Serial.print(LIGHTING[1]); Serial.print(" was : "); Serial.print(LIGHT_ACTIVE[1]); Serial.print(" - its GM43-GPIO5 is now: "); Serial.println(cstate_GM43[1]);
      pstate_GM43[1] = cstate_GM43[1];
      LIGHT_CHANGED[1]=true;
      if( LIGHT_ACTIVE[1] == "Off") { 
          digitalWrite(Relay2, HIGH); delay(2000); LIGHT_ACTIVE[1] = "On"; 
      } else { 
          digitalWrite(Relay2, LOW);  delay(2000); LIGHT_ACTIVE[1] = "Off";
      } // if( LIGHT_ACTIVE[1] == "Off") { 
      Serial.print( "Lighting has changed. " ); Serial.print(LIGHTING[1]); Serial.print(" set from Pushbuttons to : "); Serial.print(LIGHT_ACTIVE[1]); Serial.print(" - its GM43-GPIO5 is : "); Serial.println(cstate_GM43[1]);
  } // if( cstate_GM43[1] != pstate_GM43[1] )  {  

}   // void PushbuttonsPressed(boolean DoMqttWork) **************** 

void loop() {   // ****************
  if (!client.connected()) { // MQTT connection
    reconnect();
  }
  MQTTRetriesCounter = MQTTMaxRetries;
    
  ArduinoOTA.handle();
 
  String string;

  // Check if people have pressed a home pushbutton to change Lighting state 
  PushbuttonsPressed(true);

  if(  LIGHT_CHANGED[0] )  {    
      string = "{\"command\" : \"switchlight\", \"idx\" : " + LIGHT_SWITCH_IDX[0] + ", \"switchcmd\": \"" + LIGHT_ACTIVE[0] + "\"}";
      string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
      Serial.print(msgToPublish);
      Serial.print(" Published to domoticz/in. Status=");
      if ( client.publish(topic_Domoticz_IN, msgToPublish) ) { LIGHT_CHANGED[0] = false; Serial.println("OK"); } else Serial.println("KO");  
      Serial.print("GM43-GPIO4 is : "); Serial.println(cstate_GM43[0]);
  } 
  if(  LIGHT_CHANGED[1] )  {     
      string = "{\"command\" : \"switchlight\", \"idx\" : " + LIGHT_SWITCH_IDX[1] + ", \"switchcmd\": \"" + LIGHT_ACTIVE[1] + "\"}";
      string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
      Serial.print(msgToPublish);
      Serial.print(" Published to domoticz/in. Status=");
      if ( client.publish(topic_Domoticz_IN, msgToPublish) ) {  LIGHT_CHANGED[1] = false; Serial.println("OK"); } else Serial.println("KO");  
      Serial.print("GM43-GPIO5 is : "); Serial.println(cstate_GM43[1]);
  }  

  client.loop();
  delay(100);
} // void loop()  **************** 
/* THE END */

    
    


