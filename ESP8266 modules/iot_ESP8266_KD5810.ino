// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// ***** Fire Alarm server including Smoke Detector, optional Temperature Sensor and additional Indoor Siren (can be used to alert about other emergencies than fire)
//       Interfaces Smoke Detector (KD5810) using ElectroDragon IoT WiFi SPDT Relay board 
//       Get Smoke alert from Pin7 of KD5810 
//       Relay 1 used to power up (arm) and power down (disarm) Smoke Detector: COM to +9V and NO to +9V of Smoke Detector 
//       Relay 2 used to force Smoke Detector Siren to run                    : COM to +9V and NO to KD5810 pin16
//  V0.11 - April 2019
//        Initial release 

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
#define KD5810_IO_pin7               4 // KD5810 I/O pin7 connected to GPIO-5 on SPDT connector (Low=No Alarm, High=Alarm)
#define DHTPIN                      14
unsigned long timer               =  0;
 
// WiFi parameters
#define   NUMBER_OF_AP 4
byte      WiFi_AP    = 0; // The WiFi Access Point we are connected to 
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
const char*   FIRE_MQTT_ID       = "FIREALARM"; 

// Fire Alarm server internal state
enum        { DISARM=0, ARM=10, FIRE=20, SIREN=30, TEST=40 }; // Domoticz and Fire Alarm server state levels           
enum        { KD5810_ALERT, KD5810_NOALERT };                 // KD5810 I/O pin7 state levels
int         cstate_KD5810;                                    // KD5810 current I/O pin7 value
int         KD5810_FIREALARM_STATE  = DISARM;                 // Internal state value initialized to DISARM
// Fire Alarm Domoticz known state     
String      dz_FIREALARM_SWITCH_IDX = FIREALARM_SWITCH_IDX;   // Domoticz Fire Alarm device IDX (Selector)
int         dz_FIREALARM_STATE;                               // Domoticz device known state value 
#define     DZ_SYNC_TIMER           5                         // Check Domoticz synchronization and send temperature every n minutes.     
int         dz_NOT_SYNC             = -1;                     // Dz synchronized with the Fire Alarm server status: -1=Unknown; 0=OK (synchronized), 1=To resynchronize, 2=Failure    

// Temp sensor parameters
#define   ADAFRUIT_LIB  true              // Temp sensor installed. Undefine otherwise
#ifdef    ADAFRUIT_LIB
#include "DHT.h"
#define   DHTTYPE DHT22
DHT       dht(DHTPIN, DHTTYPE);
String    dz_FIREALARM_TEMP_IDX  = FIREALARM_TEMP_IDX;  // Corresponding Domoticz temp/humidity device IDX
String    dz_FIREALARM_HEAT_IDX  = FIREALARM_HEAT_IDX;  // Corresponding Domoticz temp heat index device IDX
#endif
                 
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
   Serial.println("iot_EDragon_KD5810_FIREalarm Booting - Firmware Version : 0.11");
   
#ifdef ADAFRUIT_LIB
   dht.begin();    // Set DHT items 
#endif
   pinMode(Relay1,OUTPUT);   
   pinMode(Relay2,OUTPUT);  
   digitalWrite(Relay1, LOW);      // Alarm disarm (power down)
   digitalWrite(Relay2, LOW);      // No siren 
   pinMode(KD5810_IO_pin7,INPUT); 
   cstate_KD5810 = digitalReadF(KD5810_IO_pin7,false);
   
   // connect to WiFi Access Point
   WiFi.onEvent(eventWiFi);
   WiFi.mode(WIFI_STA);
   Serial.printf( "Trying to connect to preferred WiFi AP %s\n", ssid[WiFi_AP] );
   WiFi.begin(ssid[WiFi_AP], password[WiFi_AP]);
   while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      if( ++WiFi_AP == NUMBER_OF_AP ) WiFi_AP = 0;
      delay(WiFiRetryDelay);
      Serial.printf( "Trying to connect to WiFi AP %s\n", ssid[WiFi_AP] );
      WiFi.begin(ssid[WiFi_AP], password[WiFi_AP]);
   } // while (WiFi.waitForConnectResult() != WL_CONNECTED) {

   // if ( MDNS.begin ( "esp8266" ) ) Serial.println ( "MDNS responder started" );
   
   // Port defaults to 8266
   // ArduinoOTA.setPort(8266);
   // Set OTA Hostname
   ArduinoOTA.setHostname(FIRE_MQTT_ID);  
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
   Serial.print  ( "Smoke Detector set to " ); Serial.print(KD5810_FIREALARM_STATE); Serial.print(" - KD5810_IO_pin7-GPIO5 = "); Serial.println(cstate_KD5810); 
 
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
     
   // if Domoticz message
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
        
        if ( idx == dz_FIREALARM_SWITCH_IDX ) {
             dz_NOT_SYNC = 1;
             const char* cmde = root["svalue1"];
             if( strcmp(cmde, "0") == 0 ) {
                  dz_FIREALARM_STATE     = DISARM;
                  KD5810_FIREALARM_STATE = DISARM;               
                  dz_NOT_SYNC = 0;   
                  digitalWrite(Relay1, LOW); delay(2000);
                  digitalWrite(Relay2, LOW); delay(2000);
                  Serial.println("Smoke Detector power down - DISARM state");
             } else if ( strcmp(cmde, "10") == 0 ) {
                  dz_FIREALARM_STATE=ARM;
                  if( KD5810_FIREALARM_STATE == ARM ) dz_NOT_SYNC = 0;
                  else if( KD5810_FIREALARM_STATE == DISARM ) {
                      KD5810_FIREALARM_STATE = ARM;
                      dz_NOT_SYNC = 0;
                      digitalWrite(Relay1, HIGH); delay(2000); 
                      Serial.println("Smoke Detector power up - ARM state");
                  } // if( KD5810_FIREALARM_STATE == DISARM ) {
             } else if ( strcmp(cmde, "20") == 0 ) {
                  dz_FIREALARM_STATE=FIRE;  
                  if( KD5810_FIREALARM_STATE == FIRE ) dz_NOT_SYNC = 0;
                  else if( KD5810_FIREALARM_STATE == DISARM ) { 
                           KD5810_FIREALARM_STATE = FIRE; 
                           dz_NOT_SYNC = 0;
                           digitalWrite(Relay1, HIGH); delay(2000); // Power up the Smoke Detector
                           digitalWrite(Relay2, HIGH); delay(3000); // Force siren to run
                           // digitalWrite(Relay2, LOW);  delay(2000);
                           cstate_KD5810 = digitalReadF(KD5810_IO_pin7,false);
                           Serial.print("KD5810_IO_pin7 = "); Serial.println(cstate_KD5810); 
                           Serial.println("Fire Alert from Domoticz. Smoke detector power up and put to FIRE condition - Server state to FIRE");
                  } // if( KD5810_FIREALARM_STATE == DISARM ) { 
                  else if( KD5810_FIREALARM_STATE == ARM )  { // Fire alert doesn't come from this Fire Detector. Force the siren to run       
                           KD5810_FIREALARM_STATE = FIRE;      
                           dz_NOT_SYNC = 0;                        
                           digitalWrite(Relay2, HIGH); delay(3000); 
                           // digitalWrite(Relay2, LOW);  delay(2000); 
                           cstate_KD5810 = digitalReadF(KD5810_IO_pin7,false);
                           Serial.print("KD5810_IO_pin7 = "); Serial.println(cstate_KD5810); 
                           Serial.println("Fire Alert from Domoticz. Smoke Detector put to FIRE condition - Server state to FIRE");
                  }  // else if( KD5810_FIREALARM_STATE == ARM )  { 
             } else if ( strcmp(cmde, "30") == 0 ) {
                  dz_FIREALARM_STATE=SIREN; 
                  if( KD5810_FIREALARM_STATE == SIREN ) dz_NOT_SYNC = 0;
                  else if( KD5810_FIREALARM_STATE == DISARM ) { 
                      KD5810_FIREALARM_STATE = SIREN;  
                      dz_NOT_SYNC = 0;
                      digitalWrite(Relay1, HIGH); delay(2000); // Power up the Smoke Detector
                      digitalWrite(Relay2, HIGH); delay(3000); // Force siren to run
                      // digitalWrite(Relay2, LOW);  delay(2000);
                      cstate_KD5810 = digitalReadF(KD5810_IO_pin7,false);
                      Serial.print("KD5810_IO_pin7 = "); Serial.println(cstate_KD5810); 
                      Serial.println("Other Alert than Fire from Domoticz. Smoke detector power up and put to Fire condition to run the siren - Server state to SIREN");
                  } // if( KD5810_FIREALARM_STATE == DISARM ) { 
                  else if( KD5810_FIREALARM_STATE == ARM ) {   // If no alert already set, force siren to run 
                           KD5810_FIREALARM_STATE = SIREN;
                           dz_NOT_SYNC = 0;
                           digitalWrite(Relay2, HIGH); delay(3000);
                           // digitalWrite(Relay2, LOW);  delay(2000); 
                           cstate_KD5810 = digitalReadF(KD5810_IO_pin7,false);
                           Serial.print("KD5810_IO_pin7 = "); Serial.println(cstate_KD5810); 
                           Serial.println("Other Alert than Fire from Domoticz. Smoke Detector put to Fire condition to run the siren - Server state to SIREN");
                  } // else if( KD5810_FIREALARM_STATE == ARM )                 
             } else if ( strcmp(cmde, "40") == 0 ) {
                  dz_FIREALARM_STATE=TEST; 
                  if( KD5810_FIREALARM_STATE == DISARM ) { 
                      digitalWrite(Relay1, HIGH); delay(2000); // Power up the Smoke Detector
                      digitalWrite(Relay2, HIGH); delay(3000); // Enter Test Mode
                      digitalWrite(Relay2, LOW);  delay(2000);
                      cstate_KD5810 = digitalReadF(KD5810_IO_pin7,false);
                      Serial.print("Smoke detector in Test Mode. KD5810_IO_pin7 = "); Serial.println(cstate_KD5810); 
                      digitalWrite(Relay1, LOW);  delay(2000); // Reset the Smoke Detector
                      if( cstate_KD5810 == KD5810_ALERT ) {
                          KD5810_FIREALARM_STATE = ARM;
                          Serial.println("Smoke detector Test OK");
                          digitalWrite(Relay1, HIGH); delay(2000); // Power up the Smoke Detector
                      } else Serial.println("Smoke detector Test FAILED");
                  } // if( KD5810_FIREALARM_STATE == DISARM ) { 
                  else if( KD5810_FIREALARM_STATE == ARM ) {   // If no alert already set, force siren to run 
                           digitalWrite(Relay2, HIGH); delay(3000); // Enter Test Mode
                           digitalWrite(Relay2, LOW);  delay(2000); 
                           cstate_KD5810 = digitalReadF(KD5810_IO_pin7,false);
                           Serial.print("Smoke detector in Test Mode. KD5810_IO_pin7 = "); Serial.println(cstate_KD5810); 
                           digitalWrite(Relay1, LOW);  delay(2000); // Reset the Smoke Detector
                           if( cstate_KD5810 == KD5810_NOALERT ) { KD5810_FIREALARM_STATE = DISARM; Serial.println("Smoke detector Test FAILED"); }
                           else { Serial.println("Smoke detector Test OK"); digitalWrite(Relay1, HIGH); delay(2000); } // Power up again the Smoke Detector
                  } // else if( KD5810_FIREALARM_STATE == ARM )                 
             } // if( strcmp(cmde, "40") == 0 ) {
        } // if ( idx == FIREALARM_SWITCH_IDX ) {

   } // if Domoticz message
  
   delay(15); 
   Serial.println("Leaving MQTT Callback");
} // void callback(char* to   **************** 


void reconnect() {   // ****************
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String string;
    
    // Attempt to connect
    string = "{\"command\" : \"addlogmessage\", \"message\" : \"FIREalarm went Offline - IP : " +  WiFi.localIP().toString() + "\"}";
    string.toCharArray( willMessage, MQTT_MAX_PACKET_SIZE);
    // if ( client.connect(buf) ) {
    if ( client.connect( FIRE_MQTT_ID, topic_Domoticz_IN, willQoS, willRetain, willMessage ) ) {
        Serial.println("connected");

        // suscribe to MQTT topics
        Serial.print("Subscribe to domoticz/out topic. Status=");
        if ( client.subscribe(topic_Domoticz_OUT, 0) ) Serial.println("OK"); else Serial.println("KO"); 
       
        // Wait 10 seconds to try to be sure Domoticz available after a Domoticz server reboot (i.e. means avoid MQTT available but not yet Domoticz)
        delay(10000);
                  
        // Say now "Me the Fire Alarm Server, I'm here" 
        if( ColdBoot )
             string = "{\"command\" : \"addlogmessage\", \"message\" : \"iot_EDragon_KD5810_FIREalarm Online - COLD BOOT Reason : " + ESP.getResetReason() + " - Firmware Version : 0.11 - SSID/IP/MAC : " + WiFi.SSID() + "/" + WiFi.localIP().toString() + "/" + WiFi.macAddress().c_str() + "\"}";
        else if ( WiFiWasRenew )
                  string = "{\"command\" : \"addlogmessage\", \"message\" : \"iot_EDragon_KD5810_FIREalarm Online - WIFI CONNECTION RENEWED - SSID/IP/MAC : " + WiFi.SSID() + "/" + WiFi.localIP().toString() + "/" + WiFi.macAddress().c_str() + "\"}";   
             else string = "{\"command\" : \"addlogmessage\", \"message\" : \"iot_EDragon_KD5810_FIREalarm Online - MQTT CONNECTION RENEWED - SSID/IP/MAC : " + WiFi.SSID() + "/" + WiFi.localIP().toString() + "/" + WiFi.macAddress().c_str() + "\"}";   
        string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
        Serial.print(msgToPublish);
        Serial.print("Published to domoticz/in. Status=");
        if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK"); else Serial.println("KO"); 
        ColdBoot = false; WiFiWasRenew = false;     
        
       // Ask Domoticz to publish Fire Alarm state 
       dz_NOT_SYNC = -1;
       string = "{\"command\": \"getdeviceinfo\", \"idx\": " + dz_FIREALARM_SWITCH_IDX + "}";
       string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
       Serial.print(msgToPublish);
       Serial.print("Published to domoticz/in. Status=");
       if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK"); else Serial.println("KO");                      
    } else {
        Serial.print("MQTT connection failed, rc=");
        Serial.print(client.state());
        Serial.print(", MQTTRetriesCounter="); 
        Serial.print( MQTTRetriesCounter );
        Serial.print(" - Try again in ");
        Serial.print( (int)(MQTTRetryDelay/1000) );
        Serial.println(" seconds");
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



void loop() {   // ****************
  if (!client.connected()) { // MQTT connection
    reconnect();
  }
  MQTTRetriesCounter = MQTTMaxRetries;
    
  ArduinoOTA.handle();
 
  String string;

  // Send temperature and ask DomoticZ to publish its Fire Alarm state every n minutes 
  unsigned long min = millis() / ( 60000 * DZ_SYNC_TIMER );
  if ( min < timer ) timer=min; //  millis() was reset (every 50 days)    
  if ( min > timer  ) { 
       Serial.printf("\nLast Call - Timer="); Serial.println(timer);
       Serial.print("Timer Now=");            Serial.println(min);
       timer=min;  
       if( dz_NOT_SYNC == -1 ) {
           string = "{\"command\": \"getdeviceinfo\", \"idx\": " + dz_FIREALARM_SWITCH_IDX + "}";
           string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
           Serial.print(msgToPublish);
           Serial.print("Published to domoticz/in. Status=");
           if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK"); else Serial.println("KO"); 
       } // if( dz_NOT_SYNC == -1 ) {
           
#ifdef ADAFRUIT_LIB
       char str_t[25];
       char str_h[25];
       char hstat = '0';
       char str_hic[25];
       float t = dht.readTemperature(); 
       float h = dht.readHumidity();
       float hic;

       if( isnan(t) || isnan(h) ) Serial.println("DHT22 Reading Error");
       else {
           dtostrf(t, 5, 1, str_t);  
           dtostrf(h, 5, 1, str_h); 
           hic = dht.computeHeatIndex(t, h, false); dtostrf(hic, 5, 1, str_hic);
           if( h <= 30 ) hstat ='2';
           else if( h >= 70 ) hstat='3';
           else if( h >= 45 && h <= 65 ) hstat = '1';
           
           string = "{\"idx\" : " + dz_FIREALARM_TEMP_IDX + ", \"nvalue\" : 0, \"svalue\" : \"" + str_t + ";" + str_h + ";" + hstat + "\"}"; 
           string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
           Serial.print(msgToPublish);
           Serial.print(" Published to domoticz/in. Status=");
           if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK\n"); else Serial.println("KO\n");  
           
           string = "{\"idx\" : " + dz_FIREALARM_HEAT_IDX + ", \"nvalue\" : 0, \"svalue\" : \"" + str_hic + "\"}"; 
           string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
           Serial.print(msgToPublish);
           Serial.print(" Published to domoticz/in. Status=");
           if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK\n"); else Serial.println("KO\n");   
       } // if( isnan(t) || isnan(h) )         
#endif 
  } // if ( min > timer  ) { 
  
  // Check if we have a Fire Alert !
  if( KD5810_FIREALARM_STATE == ARM ) {
      cstate_KD5810 = digitalReadF(KD5810_IO_pin7,false);
      // Serial.print("KD5810_IO_pin7 = "); Serial.println(cstate_KD5810);      
      if( cstate_KD5810 == KD5810_ALERT ) { KD5810_FIREALARM_STATE = FIRE; Serial.println("SMOKE detected"); dz_NOT_SYNC = 1; }
  }  // if( KD5810_FIREALARM_STATE == ARM ) {
  
  // Synchronize Domoticz if necessary
  if( dz_NOT_SYNC == 1 ) { 
      dz_NOT_SYNC = -1;
      string = "{\"command\" : \"switchlight\", \"idx\" : " + dz_FIREALARM_SWITCH_IDX + ", \"switchcmd\": \"Set Level\", \"level\" : " + KD5810_FIREALARM_STATE + ", \"passcode\" : " + DZ_DEVICE_PASSWORD + "}";  
      string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
      Serial.print(msgToPublish);
      Serial.print(" Published to domoticz/in. Status=");
      if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK"); else Serial.println("KO");  
  } // if( dz_FIREALARM_STATE != KD5810_FIREALARM_STATE ) {
  
  client.loop();
  delay(100);
} // void loop()  **************** 
/* THE END */

    
    


