// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// ***** Heater management and power usage logging via MQTT to DomoticZ. Corresponding Hardware is ACS712 and ElectroDragon IoT Wifi Relay board *****
// V0.851 - May 2018

#include "WiFi_OTA_MQTT_SecretKeys.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi parameters
#define   NUMBER_OF_AP 4
byte      WiFi_AP    = 1; // The WiFi Access Point we are connected to 
const     char *ssid[NUMBER_OF_AP]      = { WIFI_1,     WIFI_2,     WIFI_3,     WIFI_4 };
const     char *password[NUMBER_OF_AP]  = { WIFI_1_PWD, WIFI_2_PWD, WIFI_3_PWD, WIFI_4_PWD };
#define   MAC_LENGTH   6
byte      mac[MAC_LENGTH];
#define   MAC_UNKNOWN  "FFFFFF"
boolean   WiFiWasRenew = false;
#define   WiFiRetryDelay 5000 // Delay in ms between each WiFi connection retry

// MQTT parameters
byte          willQoS            = 0;
char          willMessage[MQTT_MAX_PACKET_SIZE+1];
boolean       willRetain         = false;
const char*   mqtt_server        = MQTT_SERVER_IP;
const char*   topic_Domoticz_IN  = "domoticz/in"; 
const char*   topic_Domoticz_OUT = "domoticz/out";         
const char*   topic_Heating_IN   = "heating/in";    
const char*   topic_Heating_OUT  = "heating/out"; 
char          msgToPublish[MQTT_MAX_PACKET_SIZE+1]; 
WiFiClient    espClient;
PubSubClient  client(espClient);
#define       MQTTRetryDelay       5000 // Delay in ms between each MQTT connection retry
#define       MQTTMaxRetriesDelay  10  // if we didn't success to reconnect in that delay in mn, then restart the ESP
const int     MQTTMaxRetries     = MQTTMaxRetriesDelay * ( 60000 / ( 4000 + MQTTRetryDelay ) ); // 4s is around the connection try duration
int           MQTTRetriesCounter = MQTTMaxRetries;        

// EDragon/ESP8266 board parameters
boolean ColdBoot                  = true;
#define FILTER                       3 // N+1 same readings every DELAY_FILTER ms must be read to validate GPIO reading ! 
#define DELAY_FILTER               100 // Delay in ms between two readings 
#define Relay1                      12 // Pin 12
#define Relay2                      13 // Pin 13
#define Thermostat_Switch           4  // Digital Pin 4  Means GPIO-5 !
#define ACS712_Voltage_Divider      4.3
byte _LOW                         = LOW;  // Reverse high/low if heater is connected to an NC relay (SPDT case)
byte _HIGH                        = HIGH; // Reverse high/low if heater is connected to an NC relay (SPDT case)

// ACS712 parameters
#define NUM_READINGS_TOCALIBRATE       5000                    // Number of readings at calibration concerning Relative digital zero 
const unsigned long sampleTime       = 100000UL;               // Sample over 100ms, it is an exact number of cycles for both 50Hz and 60Hz mains
const unsigned long numSamples       = 250UL;                  // Choose the number of samples to divide sampleTime exactly, but low enough for the ADC to keep up
const unsigned long sampleInterval   = sampleTime/numSamples;  // The sampling interval, must be longer than then ADC conversion time
float Vadc_Zero                      = ( 1024 * 2.5 ) / ACS712_Voltage_Divider; // Relative digital zero of the ESP8266 input from ACS712  (i.e 0 Amp crossing the ACS712)
int   Vadc_Zero_Min                  = 1023;
int   Vadc_Zero_Max                  = 0; 
float Arms_No_Load;                                          // To correct ACS712 unaccuracy readings at NO load situation (readings between 0.15 and 0.20 A). Readings below 1.2 * ARMS_NO_LOAD will be assumed to be zero
float NominalPowerSelfLearning;                              // Keep the Sum of all heater power readings (when heating) to get the average  
unsigned long SelfLearningNumSamples = 1;                    // 0 = NO Self Learning i.e. use Power found in DomoticZ device name
#define STOP_SELF_LEARNING             1000000               // Around 120 days (based on heater heating 12 hours per day)
int     NominalPowerOutOfRange       = 0;                    // Number of times power usage found  between 0 and 0.8 * Heater Nominal Power with SelfLearningNumSamples=1; i.e. bad heater nominal power declared here
#define FALSE_HEATER_NOMINAL_POWER     5                     // if NominalPowerOutOfRange reaches this limit, change the heater nominal power declared here

// Sensor readings management
enum          { ACS712, HLW8012, THERMOSTAT, ACS712RMS }; // Various sensors/algorithms type used -- ACS712 = ACS712 w/ Min/Max algorithm, THERMOSTAT, ACS712RMS = ACS712 w/ RootMeanSquare algorithm
#define       READINGS_PER_MINUTE  12                     // Number of power usage readings per minute
float         HEATER_CONSUMPTION_LAST_MINUTE[READINGS_PER_MINUTE];
byte          LastReadingPointer = 0;
unsigned long timer              = 0;
float         LastHeaterPower    = 0;
float         HeaterPower        = 0;

// Heaters parameters
#define DUMMY_IDX           "0"
#define NBR_HEATERS         11
int     thisHEATER;
String  HEATER_MAC6[NBR_HEATERS+1]         = { "3B1D5F", "3A73F0", "3B2071", "FA9ECE", "3B1A D",   // === MAC ADDRESS of the ESP8266/Heaters. NOTICE that trailing zero at a mac byte is converted to space, ie "3B1A0D" converted to "3B1A D"
                                               "412A10", "94D6A3", "94CD66", "94CDC2", "65DEF6", "9497B1",
                                               MAC_UNKNOWN };                                      // ** THE LAST HEATER/ESP8266 IS A UNDECLARED/MAC_UNKNOWN ONE. TO BE ABLE TO RUN THIS PROGRAM WITHOUT KNOWING ITS MAC ADDRESS THE FIRST TIME

String  HEATER_METER_IDX[NBR_HEATERS+1]    = { "27", "28", "29", "30", "31",                       // Corresponding Energy meter IDX for each heater at DomoticZ. IDX = 27 SAM, 28 Entree, 29 Cuisine, 30 Salon Sud and 31 Salon Nord
                                               "34", "35", "36", "37", "38", DUMMY_IDX,            // 34 = ECS, 35 = CH4, 36 = CH3, 37 = CH2, 38 = Parental, 53 = SDB (not installed)
                                               DUMMY_IDX };                                             // ** UNDECLARED/MAC_UNKNOWN ONE. Set to DUMMY_IDX = "0" -- 0 IS SUPPOSED TO BE AN NON EXISTING DOMOTICZ DEVICE 

float   HEATER_POWER[NBR_HEATERS+1]        = { 1426, 1426, 1384, 1261, 1261,                          // === Heaters NOMINAL POWER. Can be set to ONE (Watt) or the heater MANUFACTURER VALUE. if ONE, Self Learning will take more time to find out the right value
                                               1603, 1239, 909, 1422, 1442, 500,                   // Can also be set to ZERO, in this case we use raw value read from the power usage sensor (i.e. power usage not rounded to zero or nominal power)
                                               0 };                                                     // ** UNDECLARED/MAC_UNKNOWN. Nominal Power set to zero

String  HEATER_TYPE[NBR_HEATERS+1]         = { "Heater", "Heater", "Heater", "Heater", "Heater",      // === Heater type
                                               "HotWaterTank", "Heater", "Heater", "Heater", "Heater", "Heater",
                                               "Unknown" };         
                                               
int     HEATER_SENSOR[NBR_HEATERS+1]       = { ACS712RMS, ACS712RMS, ACS712RMS, ACS712RMS, ACS712RMS, // === SENSOR and algorithm used   ACS712 = ACS712 w/ Min/Max algorithm, THERMOSTAT, ACS712RMS = ACS712 w/ RootMeanSquare algorithm
                                               THERMOSTAT, ACS712RMS, ACS712RMS, ACS712RMS, ACS712RMS, ACS712RMS,
                                               ACS712RMS };                                                

float   HEATER_ACS712_VperA[NBR_HEATERS+1] = { 0.100, 0.100, 0.100, 0.100, 0.100,                     // === SENSIBILITY of the Heater ACS712 boards : 0.100 V/A for 20 Amp ACS712 et 0.066 V/A for 30 Amp ACS712
                                               0.100, 0.100, 0.100, 0.100, 0.100, 0.100,
                                               0.100 };                                                 // ** UNDECLARED/MAC_UNKNOWN ONE. Should be the correct value of the ACS712 connected to it 

String  HEATER_ACTIVE[NBR_HEATERS+1]       = { "On", "On", "On", "On", "On",                          // === Operator at DomoticZ side set the heater (or the zone a heater is belonging to) to UP or DOWN
                                               "On", "On", "On", "On", "On", "On",
                                               "Off" };                                                 // ** UNDECLARED/MAC_UNKNOWN ONE. Set to "Off"

String  HEATER_RELAY[NBR_HEATERS+1]        = { "NO", "NO", "NO", "NO", "NO",                          // === Heaters RELAY : NO = Normally Open and NC = Normally Connected
                                               "NC", "NO", "NO", "NO", "NO", "NO",
                                               "NO" };                                                  // ** UNDECLARED/MAC_UNKNOWN ONE.   
                                               
byte    HEATER_WIFI_AP[NBR_HEATERS+1]      = { 0, 0, 0, 0, 0,                                         // === Heaters Preferred Wifi Access Point  0=first one in *ssid[NUMBER_OF_AP], 1 =second one, ...
                                               2, 0, 1, 1, 1, 1,
                                               2 };                                                                                         
String  HEATER_MAC                = "";
float   HEATER_POWER_BOOT;               // Saved value of Heater Nominal Power declared here
int     HouseHeatingMode;                // 0=Off/10=HorsGel/20=Eco/30=Confort
String  Tariff;                          // Current tariff to use, ie Heures Pleines or Heures Creuses
long    HPWh                      = -1;  // HP and HC DomoticZ smartmeter P1 device index meter 
long    HCWh                      = -1;  // Initialized both to -1 to be sure we got at boot the stored latest values from DomoticZ before starting updating DomoticZ 
float   Decimal_Wh                = 0;   // Wh Decimal Part we can't store in P1 index meter. Kept for the next meter updates (every minute)

/* CODE */

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

// GPIO read using AC Filter
byte digitalReadF(int GPIO) {
  int i;
  byte Reading = digitalRead(GPIO);
  byte NextReading;
  for( i=1; i <= FILTER ; i++ ) {
    delay( DELAY_FILTER );
    NextReading = digitalRead(GPIO);
    if( NextReading != Reading ) { i = 1; Reading = NextReading; }
  }
  return( Reading );
} // byte digitalReadF(GPIO) {

void setup() {
   
   pinMode(Relay1,OUTPUT);   
   pinMode(Relay2,OUTPUT);  
   pinMode(Thermostat_Switch,INPUT); 

   Serial.begin(115200);
   Serial.println("iot_EDRAGON_ACS712_Heating Booting - Firmware Version : 0.851");
   
   // Find configuration main index 
   WiFi.macAddress(mac);
   for (byte i = 3; i < MAC_LENGTH; ++i) {
     char buf[3];
     sprintf(buf, "%2X", mac[i]);
     HEATER_MAC += buf;
   } // for (byte i = 3; i < MAC_LENGTH; ++i) {
   for( thisHEATER = 0; thisHEATER < NBR_HEATERS;  ) {
      if ( HEATER_MAC6[thisHEATER] == HEATER_MAC ) break;
      else thisHEATER++;  
   }
   if( HEATER_RELAY[thisHEATER] == "NC") { _LOW = HIGH; _HIGH = LOW; }
   NominalPowerSelfLearning = HEATER_POWER[thisHEATER];
   HEATER_POWER_BOOT        = HEATER_POWER[thisHEATER];
   
   // connect to WiFi Access Point
   WiFi_AP = HEATER_WIFI_AP[thisHEATER];
   WiFi.onEvent(eventWiFi);
   WiFi.mode(WIFI_STA);
   Serial.printf( "Trying to connect to preferred WiFi AP %s\n", ssid[WiFi_AP] );
   WiFi.begin(ssid[WiFi_AP], password[WiFi_AP]);
   while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      digitalWrite(Relay1,_HIGH); delay(2000); digitalWrite(Relay2,_HIGH); delay(2000); // In no WiFI is available, Activate the heaters in order to use them without Home Automation
      HEATER_ACTIVE[thisHEATER] = "On";
      if( ++WiFi_AP == NUMBER_OF_AP ) WiFi_AP = 0;
	  if( WiFiRetryDelay - 4000 > 0 ) delay( WiFiRetryDelay - 4000 );  // Already, 4s were used in delay after relays switch on.
      Serial.printf( "Trying to connect to WiFi AP %s\n", ssid[WiFi_AP] );
      WiFi.begin(ssid[WiFi_AP], password[WiFi_AP]);
   } // while (WiFi.waitForConnectResult() != WL_CONNECTED) {
   
   // Port defaults to 8266
   // ArduinoOTA.setPort(8266);

   // Set OTA Hostname
   char bufhn[MAC_LENGTH+1];
   HEATER_MAC.toCharArray(bufhn, MAC_LENGTH+1);
   ArduinoOTA.setHostname(bufhn); // "3B1D5F", "3A73F0", "3B2071", "FA9ECE", "3B1A D" :: SAM, Entree, Cuisine, Salon Sud and Salon Nord
  
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
   
   // ACS712 calibration
   if( HEATER_SENSOR[thisHEATER] == ACS712RMS) { 
       int Vadc; 
       Serial.println("Calibrating ACS712/ADC at non loading condition");
       Vadc_Zero = 0;
       digitalWrite(Relay1,_LOW); delay(2000); digitalWrite(Relay2,_LOW); delay(2000);
       //read n samples to stabilise value
       for (int i=0; i < NUM_READINGS_TOCALIBRATE; i++) {
          Vadc = analogRead(A0);
          if ( Vadc <= Vadc_Zero_Min ) Vadc_Zero_Min = Vadc;
          if  (Vadc >= Vadc_Zero_Max ) Vadc_Zero_Max = Vadc;
          Vadc_Zero += Vadc;
          delay(1);//depends on sampling (on filter capacitor), can be 1/80000 (80kHz) max.
       }  // for (int i=0; i < NUM_READINGS_TOCALIBRATE; i++) {
       Vadc_Zero /= NUM_READINGS_TOCALIBRATE;
       Arms_No_Load = ( ACS712_Voltage_Divider * 0.707 * ( (Vadc_Zero_Max - Vadc_Zero_Min) / 2 ) / 1024  ) / HEATER_ACS712_VperA[thisHEATER];
   }  // if( HEATER_SENSOR[thisHEATER] == ACS712RMS) { 
   
   if( HEATER_SENSOR[thisHEATER] == THERMOSTAT ) { 
       Vadc_Zero_Min = Vadc_Zero;
	     Vadc_Zero_Max = Vadc_Zero;
	     Arms_No_Load = ( ACS712_Voltage_Divider * 0.707 * ( (Vadc_Zero_Max - Vadc_Zero_Min) / 2 ) / 1024  ) / HEATER_ACS712_VperA[thisHEATER];
   }
   
   // Apply local heating policy declared here
   if( HEATER_ACTIVE[thisHEATER] == "On" || HEATER_RELAY[thisHEATER] == "NC" ) { digitalWrite(Relay1,_HIGH); delay(2000); digitalWrite(Relay2,_HIGH); delay(2000); }
      
   // say we are now ready and give configuration items
   Serial.println( "Ready" );
   Serial.print  ( "Connected to " );
   Serial.printf ( "%s\n", ssid[WiFi_AP] );
   Serial.print  ( "IP address: " );
   Serial.println( WiFi.localIP() );  
   Serial.print  ( "Mac address: " );
   Serial.println( HEATER_MAC );
   Serial.print  ( "HEATER index: " );
   Serial.println( thisHEATER );
   Serial.print  ( "HEATER_TYPE: ");
   Serial.println( HEATER_TYPE[thisHEATER] ); 
   Serial.print  ( "NOMINAL_HEATER_POWER: ");
   Serial.println( HEATER_POWER[thisHEATER] );   
   Serial.print  ( "HEATER_METER_IDX: " );
   Serial.println( HEATER_METER_IDX[thisHEATER] );
   Serial.print  ( "HEATER_SENSOR: " );
   Serial.println( HEATER_SENSOR[thisHEATER] );
   Serial.print  ( "HEATER_ACS712_VperA: " );
   Serial.println( HEATER_ACS712_VperA[thisHEATER] );
   Serial.print  ( "HEATER_ACS712_Relative_Digital_Vadc_Zero: " );
   Serial.println( Vadc_Zero );   
   Serial.print  ( "HEATER_ACS712_Vadc_Zero_mV: " );   
   Serial.println( map(Vadc_Zero, 0, 1023, 0, 1000) );
   Serial.print  ( "HEATER_ACS712_Relative_Digital_Vadc_Min/Max: " );
   Serial.print  ( Vadc_Zero_Min ); Serial.print("/"); Serial.println( Vadc_Zero_Max );   
   Serial.print  ( "HEATER_ACS712_Vadc_Zero_Min/Max_mV: " );   
   Serial.print  ( map(Vadc_Zero_Min, 0, 1023, 0, 1000) );  Serial.print("/");  Serial.println( map(Vadc_Zero_Max, 0, 1023, 0, 1000) );
   Serial.print  ( "HEATER_ACS712_Arms_At_No_Load_Condition using Peak Algorithm: " );   
   Serial.println( Arms_No_Load );   
   Serial.print  ( "HEATER_RELAY: " );
   Serial.println( HEATER_RELAY[thisHEATER] );

} // void setup(


void callback(char* topic, byte* payload, unsigned int length) {
   
   DynamicJsonBuffer jsonBuffer( MQTT_MAX_PACKET_SIZE );
   String messageReceived="";
   
   // Affiche le topic entrant - display incoming Topic
   Serial.print("Message arrived [");
   Serial.print(topic);
   Serial.print("] ");

   // decode payload message
   for (int i = 0; i < length; i++) {
   messageReceived+=((char)payload[i]); 
   }
   // display incoming message
   Serial.print(messageReceived);
     
   // if domoticz message
   if ( strcmp(topic, topic_Domoticz_OUT) == 0 ) {
        JsonObject& root = jsonBuffer.parseObject(messageReceived);
        if (!root.success()) {
           Serial.println("Parsing Domoticz/out JSON Received Message failed");
           return;
        }

        const char* idxChar = root["idx"];
        String idx = String( idxChar);
        // If IDX=17 (ConfortChauffage) Start or Stop the Heater according nvalue
        if ( idx == "17" ) {
             const char* cmde = root["svalue1"];
             if( strcmp(cmde, "0") == 0 ) {
                  HouseHeatingMode=0;
                  Serial.println("House Heating Mode set to OFF");
                  digitalWrite(Relay1,_LOW); delay(2000); digitalWrite(Relay2,_LOW); delay(2000); 
             } else if ( strcmp(cmde, "10") == 0 ) {
                  HouseHeatingMode=10;
                  Serial.println("House Heating Mode set to HorsGel");
                  digitalWrite(Relay1,_LOW); delay(2000); digitalWrite(Relay2,_LOW); delay(2000); 
             } else if ( strcmp(cmde, "20") == 0 ) {
                  HouseHeatingMode=20;
                  Serial.print("House Heating Mode set to Eco. This Heater policy set to ");  Serial.println( HEATER_ACTIVE[thisHEATER] );
                  if( HEATER_ACTIVE[thisHEATER] == "On" ) { digitalWrite(Relay1,_HIGH); delay(2000); digitalWrite(Relay2,_HIGH); delay(2000); }
             } else if ( strcmp(cmde, "30") == 0 ) {
                  HouseHeatingMode=30;
                  Serial.print("House Heating Mode set to Eco. This Heater policy set to ");  Serial.println( HEATER_ACTIVE[thisHEATER] );
                  if( HEATER_ACTIVE[thisHEATER] == "On" ) { digitalWrite(Relay1,_HIGH); delay(2000); digitalWrite(Relay2,_HIGH); delay(2000); }   
             }
        } // "idx" : 17
        
        // If IDX=32 (Tariff publishing) store the tariff
        if ( idx == "32" ) {      
           Tariff="HP"; // Assume Heures Pleines for now
           const char* cmde = root["nvalue"];
           if( strcmp(cmde, "0") == 0 ) Tariff="HC";
           Serial.print("Tariff is now : ");
           Serial.println(Tariff);
        } // "idx" : 32
    
        // If idx=HEATER_METER_IDX[thisHEATER] store the latest Energy meter stored for this heater at DomoticZ.
        // If Nominal Power declared in DomoticZ for this heater, use it instead of the one declared here
        if ( idx == HEATER_METER_IDX[thisHEATER] ) { 
             String string;
             const char* svalue1 = root["svalue1"];
             string = String(svalue1);
             HPWh   = string.toInt();
             const char* svalue2 = root["svalue2"];
             string = String(svalue2);
             HCWh   = string.toInt();
             Serial.print("Latest Energy HP/HC Meters in Wh stored in DomoticZ were : ");
             Serial.print(HPWh);
             Serial.print("/");
             Serial.println(HCWh);
             
             const char* sname = root["name"];
             string = String(sname);
             int posbeg = string.indexOf( "{" );
             int posend = string.indexOf( "}" );
             if( posbeg != -1 && posend != -1 && posend > posbeg+1 ) {
                 int NominalPower = string.substring(posbeg+1, posend).toInt();  // If no valid conversion could be performed because the string doesn't start with a integer number, a zero is returned
                 if( NominalPower >= 0 ) { 
                       HEATER_POWER[thisHEATER] = NominalPower;
                       NominalPowerSelfLearning = NominalPower;  // Reset Self Learning
                       SelfLearningNumSamples = 0;  
                       Serial.print("HEATER NOMINAL POWER CHANGED to the DomoticZ value. Nominal Power (Watts) is now: ");
                       Serial.println ( HEATER_POWER[thisHEATER] ); 
                 } // if( NominalPower != 0 ) {
             }  // if( posbeg != -1 && posend != -1 && posend > posbeg+1 ) {
             else if( SelfLearningNumSamples == 0 ) { // Domoticz User/Admin has just switched this Heater from Raw or Nominal Power Declared to Self Learning 
                      HEATER_POWER[thisHEATER] = HEATER_POWER_BOOT; 
                      Serial.print("HEATER NOMINAL POWER RESET to value declared locally. Nominal Power (Watts) is now: ");
                      Serial.println ( HEATER_POWER[thisHEATER] ); 
                      NominalPowerSelfLearning = HEATER_POWER_BOOT; 
                      SelfLearningNumSamples = 1;  
             } // else if( HEATER_POWER[thisHEATER]
        } // if ( strcmp(idx, HEATER_METER_IDX[thisHEATER]) == 0 ) { 
        
   } // if domoticz message
   
   // if heating message (sent by iot_Heaters.js)
   if ( strcmp(topic, topic_Heating_OUT) == 0 ) {
        JsonObject& root = jsonBuffer.parseObject(messageReceived);
        if (!root.success()) {
           Serial.println("Parsing heating/out JSON Received Message failed");
           return;
        }
        const char* HeaterUpDownChar = root[ HEATER_METER_IDX[thisHEATER] ];
        String HeaterUpDown = String( HeaterUpDownChar);
        Serial.print("received message: ");
        Serial.print(HeaterUpDown);
        Serial.println("@heating/out");
        if ( HeaterUpDown == "On" && HEATER_ACTIVE[thisHEATER] == "Off" ) {
            HEATER_ACTIVE[thisHEATER] = "On"; 
            if( HouseHeatingMode == 20 || HouseHeatingMode == 30 ) { digitalWrite(Relay1,_HIGH); delay(2000); digitalWrite(Relay2,_HIGH); delay(2000); }  
        } else if ( HeaterUpDown == "Off" && HEATER_ACTIVE[thisHEATER] == "On" ) {
            HEATER_ACTIVE[thisHEATER] = "Off"; 
            if( HouseHeatingMode == 20 || HouseHeatingMode == 30 ) { digitalWrite(Relay1,_LOW); delay(2000); digitalWrite(Relay2,_LOW); delay(2000); }
        }     
   } // if ( strcmp(topic, topic_Heating_OUT) == 0 ) 
  
   delay(15); 
} // void callback(char* to


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String string;
    
    // Attempt to connect
    char buf[MAC_LENGTH+1];
    HEATER_MAC6[thisHEATER].toCharArray(buf, MAC_LENGTH+1);
    string = "{\"command\" : \"addlogmessage\", \"message\" : \"" + HEATER_TYPE[thisHEATER] + " went Offline - mac_6@IP : " +   HEATER_MAC + "@" + WiFi.localIP().toString() + "\"}";
    string.toCharArray( willMessage, MQTT_MAX_PACKET_SIZE);
    // if ( client.connect(buf) ) {
    if ( client.connect( buf, topic_Domoticz_IN, willQoS, willRetain, willMessage ) ) {
        Serial.println("connected");    
        
        // suscribe to MQTT topics
        Serial.print("Subscribe to domoticz/out topic. Status=");
        if ( client.subscribe(topic_Domoticz_OUT, 0) ) Serial.println("OK"); else Serial.println("KO"); 
        Serial.print("Subscribe to heating/out topic. Status=");
        if ( client.subscribe(topic_Heating_OUT, 0) ) Serial.println("OK"); else Serial.println("KO"); 
        Serial.println("");
  
        // Say now "Me the heater, I'm here" and then get the context 
        if( ColdBoot )
             string = "{\"command\" : \"addlogmessage\", \"message\" : \"New " + HEATER_TYPE[thisHEATER] + " Online - COLD BOOT Reason : " + ESP.getResetReason() + " - Firmware Version : 0.851 - SSID/IP/MAC : " + WiFi.SSID() + "/" + WiFi.localIP().toString() + "/" + WiFi.macAddress().c_str() +
                                                                                                           " - ACS712 Calibration Vadc_Zero/Vadc_Zero_Min/Vadc_Zero_Max/Arms_No_Load : " + Vadc_Zero + "/" + Vadc_Zero_Min + "/" + Vadc_Zero_Max + "/" + Arms_No_Load + "\"}";
        else if ( WiFiWasRenew )
                  string = "{\"command\" : \"addlogmessage\", \"message\" : \"New " + HEATER_TYPE[thisHEATER] + " Online - WIFI CONNECTION RENEWED - SSID/IP/MAC : " + WiFi.SSID() + "/" + WiFi.localIP().toString() + "/" + WiFi.macAddress().c_str() + "\"}";   
             else string = "{\"command\" : \"addlogmessage\", \"message\" : \"New " + HEATER_TYPE[thisHEATER] + " Online - MQTT CONNECTION RENEWED - SSID/IP/MAC : " + WiFi.SSID() + "/" + WiFi.localIP().toString() + "/" + WiFi.macAddress().c_str() + "\"}";   
        string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
        Serial.print(msgToPublish);
        Serial.print("Published to domoticz/in. Status=");
        if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK"); else Serial.println("KO"); 
        WiFiWasRenew = false;
        
        // Ask DomoticZ to publish Heating mode 
        strcpy(msgToPublish,"{\"command\": \"getdeviceinfo\", \"idx\": 17}");
        Serial.print(msgToPublish);
        Serial.print("Published to domoticz/in. Status=");
        if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK"); else Serial.println("KO");   
        
        // Ask DomoticZ to publish Curent Tariff HP or HC 
        strcpy(msgToPublish,"{\"command\": \"getdeviceinfo\", \"idx\": 32}");
        Serial.print(msgToPublish);
        Serial.print("Published to domoticz/in. Status=");
        if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK"); else Serial.println("KO");    
      
        // Ask DomoticZ to publish last power usage stored for this heater
        if( ColdBoot ) {
          if( HEATER_METER_IDX[thisHEATER] != DUMMY_IDX ) {
              string="{\"command\": \"getdeviceinfo\", \"idx\": " + HEATER_METER_IDX[thisHEATER] + "}";
              string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
              Serial.print(msgToPublish);
              Serial.print("Published to domoticz/in. Status=");
              if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK"); else Serial.println("KO");  
          } // if( HEATER_METER_IDX[thisHEATER] != DUMMY_IDX
          ColdBoot = false; 
        } // if( ColdBoot ) {
        
    } else {
        Serial.print("MQTT connection failed, rc=");
        Serial.print(client.state());
        Serial.print(", MQTTRetriesCounter="); 
        Serial.print( MQTTRetriesCounter );
        Serial.print(" - Try again in ");
        Serial.print( (int)(MQTTRetryDelay/1000) );
        Serial.println(" seconds");
        digitalWrite(Relay1,_HIGH); delay(2000); digitalWrite(Relay2,_HIGH); delay(2000); // In case DomoticZ/MQTT server is down, Activate the heaters in order to use them without Home Automation
        HEATER_ACTIVE[thisHEATER] = "On";
        if( MQTTRetriesCounter-- == 0 ) ESP.reset();
        // Wait n seconds before retrying
        if( MQTTRetryDelay - 4000 > 0 ) delay( MQTTRetryDelay - 4000 );  // Already, 4s were used in delay after relays switch on.
    }  // if (client.connect
    
  } // while (!client.connected()) {
} // void reconnect() {


void loop() {   
  
  if (!client.connected()) { // MQTT connection monitor
    reconnect();
  }
  MQTTRetriesCounter = MQTTMaxRetries;
  
  ArduinoOTA.handle();
    
  // Read power usage sensor every n seconds and send it to DomoticZ every minute
  LastHeaterPower = HeaterPower;
  unsigned long min = millis() / ( 60000 / READINGS_PER_MINUTE );
  if ( min < timer ) timer=min; //  millis() was reset (every 50 days)    
	if ( min > timer  ) {

        Serial.printf("\nLast Call - Timer="); Serial.println(timer);
        Serial.print("Timer Now=");            Serial.println(min);
        timer=min;  
        
        if( HEATER_SENSOR[thisHEATER] == ACS712RMS) {     
            unsigned long currentAcc = 0;
            unsigned int count       = 0;
            int adc_raw              = 0;
            int Vadc_Min ;    // Max Voltage digital calculated
            int Vadc_Max ;    // Min Voltage digital calculated
            unsigned long prevMicros = micros() - sampleInterval;
            while (count < numSamples) {
               if( micros() - prevMicros >= sampleInterval ) {
                   adc_raw = analogRead(A0);
                   Vadc_Zero = Vadc_Zero + ( adc_raw - Vadc_Zero)/1024; // Low Pass Filter to update/adjust Vadc_Zero
                   adc_raw -= Vadc_Zero;
                   currentAcc += (unsigned long)(adc_raw * adc_raw);
                   ++count;
                   prevMicros += sampleInterval;
               }  // if (micros() - prevMicros >= sampleInterval) {
            } // while (count < numSamples) {
            // Calculate now Arms
            // The ratio conversion comes from solving for X : ((ESP8266 max analog input voltage * ACS712_Voltage_Divider) / X) = (ACS712 sensitivity / 1 A)
            // i.e. the output of the ACS712-20 (the 20A version) is 100mV/A, so 4.3V would correspond to 43A, and the ratio conversion would be 43.0 to get the output in amps.
            float Arms = sqrt((float)currentAcc/(float)numSamples) * ( ( ACS712_Voltage_Divider * 1 / HEATER_ACS712_VperA[thisHEATER] ) / 1024.0 );
            Serial.print("RAW READING Heater AC Current in A is : ");  Serial.println(Arms); 
            if( Arms < (1.2 * Arms_No_Load) ) Arms = 0; // ACS712 readings at no load situation are between 0.15 and 0.20 A. Readings below Arms_No_Load are assumed to be zero
            Vadc_Max = Vadc_Zero + ( ( 1024 * Arms * HEATER_ACS712_VperA[thisHEATER] ) / (ACS712_Voltage_Divider * 0.707 ) ); Vadc_Min = Vadc_Zero - ( Vadc_Max - Vadc_Zero );
            if( Vadc_Min > Vadc_Max ) Vadc_Min = Vadc_Max;
            HeaterPower = 230*Arms;
            Serial.print("New Filtered Vadc_Zero value : ");      Serial.println(Vadc_Zero);      
            Serial.print("Vadc_Min corresponding value : ");      Serial.println(Vadc_Min);      
            Serial.print("Vadc_Max corresponding value : ");      Serial.println(Vadc_Max);      
            Serial.print("Heater AC Current in A is : ");         Serial.println(Arms);      
            Serial.print("Heater Power usage in W is : ");        Serial.println(HeaterPower);    
            if( HEATER_POWER[thisHEATER] != 0 ) { // If Non ACS712 raw mode, i.e. using Heater Nominal Power Self Learing or Heater Nominal Power declared in DomoticZ device name
                if( HeaterPower >= 0.8 * HEATER_POWER[thisHEATER] ) {
                    if( SelfLearningNumSamples <= STOP_SELF_LEARNING ) NominalPowerSelfLearning += HeaterPower; 
                    HeaterPower = HEATER_POWER[thisHEATER];                   
                    if( SelfLearningNumSamples != 0 && SelfLearningNumSamples <= STOP_SELF_LEARNING ) { 
                        HEATER_POWER[thisHEATER] = NominalPowerSelfLearning / ++SelfLearningNumSamples;
                        HeaterPower = HEATER_POWER[thisHEATER];
                        Serial.print( "NEW HEATER NOMINAL POWER TARGET (Self Learning Update Number): "); Serial.print( HEATER_POWER[thisHEATER] ); Serial.print(" ("); Serial.print( SelfLearningNumSamples ); Serial.println(")"); 
                    } // if( SelfLearningNumSamples != 0 ) {                                                     
                } // if( HeaterPower >= 0.8 * HEATER_POWE
                else if( HeaterPower > 0 ) {  // Assuming Heater switching from non heating to heating or the opposite 
                         if( LastHeaterPower == 0 ) HeaterPower=HEATER_POWER[thisHEATER]; 
                         else { 
                             HeaterPower=0; 
                             if( SelfLearningNumSamples == 1 )
                                 if( ++NominalPowerOutOfRange == FALSE_HEATER_NOMINAL_POWER ) { 
                                     HEATER_POWER[thisHEATER] /= 2;
                                     NominalPowerSelfLearning = HEATER_POWER[thisHEATER];
                                     NominalPowerOutOfRange = 0;
                                     Serial.print( "NEW HEATER NOMINAL POWER TARGET (Self Learning Update Number): "); Serial.print( HEATER_POWER[thisHEATER] ); Serial.print(" ("); Serial.print( SelfLearningNumSamples ); Serial.println(")");
                                 }  // if( ++NominalPowerOutOfRange == FALSE_HEATER_NOMIN
                         } // if( LastHeaterPower == 0 )
                } // else if( HeaterPower > 0 ) {    
                Vadc_Max = Vadc_Zero + ( ( 1024 * (HeaterPower/230) * HEATER_ACS712_VperA[thisHEATER] ) / (ACS712_Voltage_Divider * 0.707 ) ); Vadc_Min = Vadc_Zero - ( Vadc_Max - Vadc_Zero ); 
                if( Vadc_Min > Vadc_Max ) Vadc_Min = Vadc_Max;  
                Serial.print("FILTERED Vadc_Min corresponding value : ");      Serial.println(Vadc_Min);      
                Serial.print("FILTERED Vadc_Max corresponding value : ");      Serial.println(Vadc_Max);                  
                Serial.print("FILTERED Heater AC Current in A is : ");         Serial.println(HeaterPower/230);      
                Serial.print("FILTERED Heater Power usage in W is : ");        Serial.println(HeaterPower);
            } // if( HEATER_POWER[thisHEATER] != 0 )
            HEATER_CONSUMPTION_LAST_MINUTE[LastReadingPointer++] = HeaterPower;
            if( LastReadingPointer == READINGS_PER_MINUTE) {             
                // Log to Monitoring topic for monitoring/debugging the analog/digital ADC values read
                String string = "{\"command\" : \"addlogmessage\", \"message\" : \"idx : " +   HEATER_METER_IDX[thisHEATER] + ", HeaterActive=" + HEATER_ACTIVE[thisHEATER] + ", Vadc_Min/Max=" + Vadc_Min + "/" + Vadc_Max + "\"}";
                string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
                Serial.print(msgToPublish);
                Serial.print(" Published to heating/in Topic. Status=");
                if ( client.publish(topic_Heating_IN, msgToPublish) ) Serial.println("OK"); else Serial.println("KO"); 
            } // if( LastReadingPointer == READINGS_PER_MINUTE) { 
        }  // if( HEATER_METER_IDX[thisHEATER] == ACS712RMS ) {          
              
        if( HEATER_SENSOR[thisHEATER] == THERMOSTAT ) {    
            // read Thermostat switch via GPIO
            byte Thermostat_GPIO = digitalReadF(Thermostat_Switch);
            if( Thermostat_GPIO == 0 ) HeaterPower = HEATER_POWER[thisHEATER]; else HeaterPower = 0; 
            HEATER_CONSUMPTION_LAST_MINUTE[LastReadingPointer++] = HeaterPower;
            float Arms = (float)HeaterPower / 230;
            int Vadc_Max = Vadc_Zero + ( ( 1024 * Arms * HEATER_ACS712_VperA[thisHEATER] ) / (ACS712_Voltage_Divider * 0.707 ) ); int Vadc_Min = Vadc_Zero - ( Vadc_Max - Vadc_Zero);
            if( Vadc_Min > Vadc_Max ) Vadc_Min = Vadc_Max;
            Serial.print("Thermostat(O=Close.ON/1=Open.OFF) : "); Serial.println(Thermostat_GPIO);   
            Serial.print("Vadc_Min corresponding value : ");      Serial.println(Vadc_Min);      
            Serial.print("Vadc_Max corresponding value : ");      Serial.println(Vadc_Max);      
            Serial.print("Heater AC Current in A is : ");         Serial.println(Arms);      
            Serial.print("Heater Power usage in W is : ");        Serial.println(HeaterPower);
            if( LastReadingPointer == READINGS_PER_MINUTE) {    
                // Log to Monitoring topic for monitoring/debugging the thermostat status
                // String string = "{\"command\" : \"addlogmessage\", \"message\" : \"idx : " +   HEATER_METER_IDX[thisHEATER] + ", HeaterActive=" + HEATER_ACTIVE[thisHEATER] + ", Thermostat(O=Close/1=Open)=" + Thermostat_GPIO + "\"}";
                String string = "{\"command\" : \"addlogmessage\", \"message\" : \"idx : " +   HEATER_METER_IDX[thisHEATER] + ", HeaterActive=" + HEATER_ACTIVE[thisHEATER] + ", Vadc_Min/Max=" + Vadc_Min + "/" + Vadc_Max + "\"}";
                string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
                Serial.print(msgToPublish);
                Serial.print(" Published to heating/in Topic. Status=");
                if ( client.publish(topic_Heating_IN, msgToPublish) ) Serial.println("OK"); else Serial.println("KO");  
            } // if( LastReadingPointer == READINGS_PER_MINUTE) { 
        } // if( HEATER_METER_IDX[thisHEATER] == THERMOSTAT ) {    
              
        // send now new Power usage index to DomoticZ        
        if( LastReadingPointer == READINGS_PER_MINUTE) {  
            for( LastReadingPointer = 0, HeaterPower=0 ; LastReadingPointer < READINGS_PER_MINUTE; LastReadingPointer++ ) HeaterPower += HEATER_CONSUMPTION_LAST_MINUTE[LastReadingPointer]; 
            HeaterPower /= READINGS_PER_MINUTE;
            LastReadingPointer = 0;     
            if( HEATER_METER_IDX[thisHEATER] != DUMMY_IDX ) {
                if( HPWh != -1 && HCWh != -1 ) {  // HPWh or HCWh equal to -1 means we didn't receive at boot (MQTT connection) the latest Energy meter stored for this heater at DomoticZ.
                                                  // Can only happen if at boot time, MQTT was available but not yet DomoticZ     
                    if ( Tariff == "HP" ) HPWh = HPWh + HeaterPower/60 + (int)Decimal_Wh;      // we are here every minute, so additional Wh usage to add every minute to the meter is Power/60
                    else HCWh = HCWh + HeaterPower/60  + (int)Decimal_Wh;
                    Serial.print("\n== Updating Domoticz. Average Power used : "); Serial.print(HeaterPower);
                    Serial.print(", Wh Usage added to index meter : ");      Serial.print( (int)(HeaterPower/60) + (int)Decimal_Wh );                                    
                    Decimal_Wh = Decimal_Wh - (int)Decimal_Wh + HeaterPower/60 - (int)( HeaterPower/60 );   // Keep Wh decimal part for next updates
                    Serial.print(", Wh Decimal Part kept for next update : ");     Serial.println(Decimal_Wh);
                    String string = "{\"idx\" : " + HEATER_METER_IDX[thisHEATER] + ", \"nvalue\" : 0, \"svalue\" : \"" + HPWh + ";" + HCWh + ";0;0;" + (int)HeaterPower + ";0\"}"; 
                    string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
                    Serial.print(msgToPublish);
                    Serial.print(" Published to domoticz/in. Status=");
                    if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK\n"); else Serial.println("KO\n");  
                } else {
                    // Ask again DomoticZ to publish Heating mode, Curent Tariff HP or HC and Index meter for this heater 
                    strcpy(msgToPublish,"{\"command\": \"getdeviceinfo\", \"idx\": 17}");
                    Serial.print(msgToPublish);
                    Serial.print("Published to domoticz/in. Status=");
                    if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK"); else Serial.println("KO");   
                    strcpy(msgToPublish,"{\"command\": \"getdeviceinfo\", \"idx\": 32}");
                    Serial.print(msgToPublish);
                    Serial.print("Published to domoticz/in. Status=");
                    if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK"); else Serial.println("KO");    
                    String string="{\"command\": \"getdeviceinfo\", \"idx\": " + HEATER_METER_IDX[thisHEATER] + "}";
                    string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
                    Serial.print(msgToPublish);
                    Serial.print("Published to domoticz/in. Status=");
                    if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK"); else Serial.println("KO");                 
                } // if( HPWh != -1 && HCWh != -1    
            } // if( HEATER_METER_IDX[thisHEATER] != DUMMY_IDX )           
        } // if( LastReadingPointer == READINGS_PER_MINUTE) {  
  } // if ( min > timer  ) {
    
  client.loop();
  delay(100);
  
} // void loop()   
/* CODE - END */

    
    


