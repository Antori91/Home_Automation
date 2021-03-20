// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// ***** Fire Alarm server including Smoke Detector, optional Temperature Sensor and additional Indoor Siren (can be used to alert about other emergencies than fire)
//       Configuration supported:
//       1. Smoke Detector appliance (KD5810) using ElectroDragon IoT WiFi SPDT Relay board 
//          Get Smoke alert from Pin7 of KD5810 
//          Relay 1 used to power up (arm) and power down (disarm) Smoke Detector: COM to +9V and NO to +9V of Smoke Detector 
//          Relay 2 used to force Smoke Detector Siren to run                    : COM to +9V and NO to KD5810 pin16
//       2. MQ-2 sensor and standalone alarm siren using ElectroDragon IoT WiFi SPST Relay board
//          Get Smoke value from Ao pin using ESP8266 ADC
//          Relay1 used to run the siren (230 AC to +12V adapter)        
//  V0.20 - March 2021
//        New configuration in addition to KD5810 smoke detector appliance supported: MQ-2 sensor and standalone alarm siren
//        RL_VALUE of 1 kilo ohm used to apply a maximum around 1V at the ESP8266 ADC input 
//        MQ-2 core routines: Tiequan Shao (http://sandboxelectronics.com/?p=165) - 
//        MQ-2 core routines changes: ESP8266 ADC parameters (MQ-2 VCC = 5*1023) and log10 used to compute ppm from the sensitivity characteristics 
//  V0.12 - April 2019
//        Change: increase KD5810 I/O pin filtering time - Allows KD5810 to go back to non smoke condition
//        Improvement: raise Domoticz failure flag if DHT22 Temp sensor failure 
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
#define FILTER                       5 // N+1 (150 ms) same readings every DELAY_FILTER ms must be read to be sure about Smoke Detector I/O status 
#define DELAY_FILTER                25 // Delay in ms between two readings
#define Relay1                      12 // Digital Pin 12
#define Relay2                      13 // Digital Pin 13
#define KD5810_IO_pin7               4 // KD5810 I/O pin7 connected to GPIO-5 on SPDT connector (Low=No Alarm, High=Alarm)
#define DHTPIN                      14
unsigned long timer               =  0;
 
// WiFi parameters
#define   NUMBER_OF_AP 4
byte      WiFi_AP    = 1; // The WiFi Access Point we are connected to 
const     char *ssid[NUMBER_OF_AP]      = { WIFI_1,     WIFI_2,     WIFI_3,     WIFI_4 };
const     char *password[NUMBER_OF_AP]  = { WIFI_1_PWD, WIFI_2_PWD, WIFI_3_PWD, WIFI_4_PWD };
boolean   WiFiWasRenew = false;
#define   WiFiRetryDelay 2000 // Delay in ms between each WiFi connection retry

// MQTT parameters
byte          willQoS            = 0;
char          willMessage[MQTT_MAX_PACKET_SIZE+1];
boolean       willRetain         = false;
const char*   mqtt_server        = MQTT_SERVER_IP;
const char*   topic_Domoticz_IN  = DZ_IN_TOPIC; 
const char*   topic_Domoticz_OUT = DZ_OUT_TOPIC;         
char          msgToPublish[MQTT_MAX_PACKET_SIZE+1]; 
WiFiClient    espClient;
PubSubClient  client(espClient);
#define       MQTTRetryDelay       2000 // Delay in ms between each MQTT connection retry
#define       MQTTMaxRetriesDelay  10  // if we didn't success to reconnect in that delay in mn, then restart the ESP
const int     MQTTMaxRetries     = MQTTMaxRetriesDelay * ( 60000 / ( 4000 + MQTTRetryDelay + FILTER*2*DELAY_FILTER ) ); // 4s is around the connection try duration
int           MQTTRetriesCounter = MQTTMaxRetries;  
const char*   FIRE_MQTT_ID       = "MQ2FIREALARM"; 

// Fire Alarm server internal state
//#define   KD5810_SENSOR_USED  true   
#define     MQ2_SENSOR_USED     true 
enum        { DISARM=0, ARM=10, FIRE=20, SIREN=30, TEST=40 }; // Domoticz and Fire Alarm server state levels            
#ifdef KD5810_SENSOR_USED
enum        { KD5810_ALERT, KD5810_NOALERT };                 // KD5810 I/O pin7 state levels
int         cstate_KD5810;                                    // KD5810 current I/O pin7 value
#endif
int         FIREALARM_STATE  = DISARM;                        // Internal state value initialized to DISARM

// Fire Alarm Domoticz known state     
String      dz_FIREALARM_SWITCH_IDX = FIREALARM_SWITCH_IDX;   // Domoticz Fire Alarm device IDX (Selector)
int         dz_FIREALARM_STATE;                               // Domoticz device known state value 
#define     DZ_SYNC_TIMER           1                         // Check Domoticz synchronization and send data every n minutes.     
int         dz_NOT_SYNC             = -1;                     // Dz synchronized with the Fire Alarm server status: -1=Unknown; 0=OK (synchronized), 1=To resynchronize, 2=Failure    

// Temp sensor parameters
#define   ADAFRUIT_LIB  true                                  // Temp sensor installed. Undefine otherwise
#ifdef    ADAFRUIT_LIB
#include "DHT.h"
#define   DHTTYPE DHT22
DHT       dht(DHTPIN, DHTTYPE);
String    dz_FIREALARM_TEMP_IDX  = FIREALARM_TEMP_IDX;        // Corresponding Domoticz temp/humidity device IDX
String    dz_FIREALARM_HEAT_IDX  = FIREALARM_HEAT_IDX;        // Corresponding Domoticz temp heat index device IDX
String    dz_TEMPFAILUREFLAG_IDX = IDX_TEMPFAILUREFLAG;       // Domoticz Temp sensor Alarm device IDX
int       TempSensorTimeOut      = 2 * (60 / DZ_SYNC_TIMER);  // If Temp Sensor is not available for n hours, raise Domoticz failure flag
#endif

// MQ-2 parameters and routines 
#ifdef MQ2_SENSOR_USED
String          dz_FIREALARM_SMOKE_IDX       = FIREALARM_SMOKE_IDX; // Corresponding Domoticz smoke device IDX
#define         SMOKE_ALERT                  (300)  // Default Smoke alert threshold in ppm
#define         MQ_PIN                       (A0)   // define analog input channel used
#define         RL_VALUE                     (0.66) // define the load resistance on the board, in kilo ohms
#define         RO_CLEAN_AIR_FACTOR          (9.83) // RO_CLEAR_AIR_FACTOR=(Sensor resistance in clean air)/Ro,
#define         CALIBRATION_SAMPLE_TIMES     (25)   // define how many samples taken in the calibration phase
#define         CALIBRATION_SAMPLE_INTERVAL  (500)  // define the time interval(in millisecond) between each samples in the calibration phase
#define         READ_SAMPLE_TIMES            (5)    // define how many samples taken in normal operation 
#define         READ_SAMPLE_INTERVAL         (50)   // define the time interval(in millisecond) between each samples in normal operation
#define         GAS_LPG                      (0)
#define         GAS_CO                       (1)
#define         GAS_SMOKE                    (2)
float           LPGCurve[3]   =  {2.3,0.21,-0.47};  // two points are taken from the curve. With these two points, a line is formed which is "approximately equivalent" to the original curve. 
                                                    // LPG data { x, y, slope};   point1: (log10(200), 0.21), point2: (log10(10000), -0.59) 
float           COCurve[3]    = {2.3,0.72,-0.34};   // CO data { x, y, slope};    point1: (log10(200), 0.72), point2: (log10(10000),  0.15)                                                    
float           SmokeCurve[3] = {2.3,0.53,-0.44};   // Smoke data { x, y, slope}; point1: (x=log10(200)=2.30, y=log10(3,4)=0.53), point2: (x=log10(10000)=4, y=log10(0,6)=-0.22)                                                    
float           Ro                    = 10;         // Ro is initialized to 10 kilo ohms
int             MQ2_Raw_adc           = 0;          // Last MQ-2 ADC reading
float           Smoke                 = 0;          // Max MQ-2 smoke computed value for the last minute
int             Smoke_alert_threshold = SMOKE_ALERT;// Smoke ppm threshold to detect potential fire! Threshold can be change on the fly using Domoticz device name (dz_FIREALARM_SWITCH_IDX): D�tection INCENDIE{threshold to use} 
boolean         MQ2calibration        = false;      // MQ2 calibration just done. Result has to be logged in Domoticz

// Compute current sensor resistance
float MQResistanceCalculation(int raw_adc) {
   // return ( ((float)RL_VALUE*(1023-raw_adc)/raw_adc)); 
   return ( ((float)RL_VALUE*(5*1023-raw_adc)/raw_adc)); 
}
 
// Calibration in clean air - Compute Ro of the sensor
float MQCalibration(int mq_pin) {
  Serial.println("Calibrating MQ-2 sensor...");     
  int   i;
  float val=0; 
  int   _raw_adc;
  float _raw_Rs; 
  for (i=0;i<CALIBRATION_SAMPLE_TIMES;i++) {            //take multiple samples
    _raw_adc = analogRead(mq_pin);
    _raw_Rs = MQResistanceCalculation(_raw_adc);
    val += _raw_Rs;
    Serial.print("Reading #"); Serial.print(i); Serial.print(" _raw_adc/_raw_Rs = "); Serial.print(_raw_adc); Serial.print("/"); Serial.println(_raw_Rs);  
    delay(CALIBRATION_SAMPLE_INTERVAL);
  }
  val = val/CALIBRATION_SAMPLE_TIMES;                   //calculate the average value
  Serial.print("Average(_raw_Rs) = "); Serial.println(val);
  val = float( long(val/RO_CLEAN_AIR_FACTOR * 1000) ) / 1000.0; //divided by RO_CLEAN_AIR_FACTOR yields the Ro according to the chart in the datasheet 
  Serial.print("MQ-2 sensor calibration done. Ro = "); Serial.print(val); Serial.println(" kohm");
  MQ2calibration = true;  
  return val;                     
}

// Main routine - Sensor reading - Return Rs of the sensor
float MQRead(int mq_pin) {
  int i;
  float rs=0;
  for (i=0;i<READ_SAMPLE_TIMES;i++) {
    MQ2_Raw_adc = analogRead(mq_pin);
    if( MQ2_Raw_adc > 700 ) { Serial.print("MQ2_Raw_adc reading in High Range. Value = "); Serial.println(MQ2_Raw_adc); } // To monitor A0 voltage not greater than 1V (i.e correct value choosen for the load resistance on the MQ-2 board)
    rs += MQResistanceCalculation(MQ2_Raw_adc);
    delay(READ_SAMPLE_INTERVAL);
  }
  return rs/READ_SAMPLE_TIMES;  
}
 
// Compute ppm for all gas supported by the MQ-2
int MQGetGasPercentage(float rs_ro_ratio, int gas_id) {
  if ( gas_id == GAS_LPG ) {
     return MQGetPercentage(rs_ro_ratio,LPGCurve);
  } else if ( gas_id == GAS_CO ) {
     return MQGetPercentage(rs_ro_ratio,COCurve);
  } else if ( gas_id == GAS_SMOKE ) {
     return MQGetPercentage(rs_ro_ratio,SmokeCurve);
  }     
  return 0;
}
 
// Compute ppm from the sensitivity characteristics of the MQ-2
int  MQGetPercentage(float rs_ro_ratio, float *pcurve) {
  // return (pow(10,( ((log(rs_ro_ratio)-pcurve[1])/pcurve[2]) + pcurve[0])));
  return (pow(10,( ( log10(rs_ro_ratio)-pcurve[1] ) / pcurve[2] + pcurve[0] )));
}
#endif

// WiFi routines
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
   Serial.println("iot_EDragon_FIREalarm Booting - Firmware Version : 0.20");
   
#ifdef ADAFRUIT_LIB
   dht.begin();    // Set DHT items 
#endif
   pinMode(Relay1,OUTPUT);   
   pinMode(Relay2,OUTPUT);  
   digitalWrite(Relay1, LOW);      
   digitalWrite(Relay2, LOW);      
#ifdef KD5810_SENSOR_USED
   pinMode(KD5810_IO_pin7,INPUT); 
   cstate_KD5810 = digitalReadF(KD5810_IO_pin7,false);
#endif
   
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
   client.setServer(mqtt_server, MQTT_PORT);
   client.setCallback(callback);

   // MQ-2 sensor calibration
#ifdef MQ2_SENSOR_USED           
   Ro = MQCalibration(MQ_PIN);                                        
#endif

   // say we are now ready and give configuration items
   Serial.println( "Ready" );
   Serial.print  ( "Connected to " ); Serial.printf ( "%s\n", ssid[WiFi_AP] );
   Serial.print  ( "IP address: " );  Serial.println( WiFi.localIP() );  
#ifdef KD5810_SENSOR_USED   
   Serial.print  ( "Smoke Detector set to " ); Serial.print(FIREALARM_STATE); Serial.print(" - KD5810_IO_pin7-GPIO5 = "); Serial.println(cstate_KD5810); 
#endif
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
   Serial.println(messageReceived);
     
   // if Domoticz message
   if ( strcmp(topic, topic_Domoticz_OUT) == 0 ) {
        Serial.println("MQTT Callback. Parsing Domoticz Message arrived...");
        JsonObject& root = jsonBuffer.parseObject(messageReceived);
        if (!root.success()) {
           Serial.println("Parsing Domoticz/out JSON received Message failed");
           return;
        }

        const char* idxChar = root["idx"];
        String idx = String( idxChar);
        Serial.print("MQTT Callback. This message is about Domoticz Device Idx=");Serial.println(idx);       
        
        if ( idx == dz_FIREALARM_SWITCH_IDX ) {
             dz_NOT_SYNC = 1;
#ifdef MQ2_SENSOR_USED
             const char* sname = root["name"];
             String string = String(sname);
             int posbeg = string.indexOf( "{" );
             int posend = string.indexOf( "}" );
             if( posbeg != -1 && posend != -1 && posend > posbeg+1 ) Smoke_alert_threshold = string.substring(posbeg+1, posend).toInt();  
             if( Smoke_alert_threshold == 0 ) Smoke_alert_threshold = SMOKE_ALERT;
#endif
             const char* cmde = root["svalue1"];
             if( strcmp(cmde, "0") == 0 ) {     // OFF
                  dz_FIREALARM_STATE = DISARM;
                  FIREALARM_STATE    = DISARM;               
                  dz_NOT_SYNC = 0;   
                  digitalWrite(Relay1, LOW); delay(2000);
                  digitalWrite(Relay2, LOW); delay(2000);
                  Serial.println("Smoke Detector DISARMED");
             } else if ( strcmp(cmde, "10") == 0 ) {  // ARM/SURVEILLANCE
                  dz_FIREALARM_STATE=ARM;
                  if( FIREALARM_STATE == ARM ) dz_NOT_SYNC = 0;
                  else if( FIREALARM_STATE == DISARM ) {
                      FIREALARM_STATE = ARM;
                      dz_NOT_SYNC = 0;
#ifdef KD5810_SENSOR_USED 
                      digitalWrite(Relay1, HIGH); delay(2000);  // Power up the KD5810 Smoke Detector.
#endif
                      Serial.println("Smoke Detector ARMED");
                  } // if( FIREALARM_STATE == DISARM ) {
             } else if ( strcmp(cmde, "20") == 0 ) {  // SMOKE/FUMEE
                  dz_FIREALARM_STATE = FIRE;  
                  if( FIREALARM_STATE == FIRE ) dz_NOT_SYNC = 0;
                  else if( FIREALARM_STATE == DISARM ) { 
                           FIREALARM_STATE = FIRE; 
                           dz_NOT_SYNC = 0;
#ifdef MQ2_SENSOR_USED
                           digitalWrite(Relay1, HIGH); delay(2000); // Run the siren
#endif
#ifdef KD5810_SENSOR_USED
                           digitalWrite(Relay1, HIGH); delay(2000); // Power up the KD5810 Smoke Detector
                           digitalWrite(Relay2, HIGH); delay(3000); // Force siren to run
                           // digitalWrite(Relay2, LOW);  delay(2000);
                           cstate_KD5810 = digitalReadF(KD5810_IO_pin7,false);
                           Serial.print("KD5810_IO_pin7 = "); Serial.println(cstate_KD5810); 
#endif
                           Serial.println("Fire Alert from Domoticz. Smoke detector ARMED - Server state to FIRE");
                  } // if( FIREALARM_STATE == DISARM ) { 
                  else if( FIREALARM_STATE == ARM )  { // Fire alert doesn't come from this Fire Detector. Force the siren to run       
                           FIREALARM_STATE = FIRE;      
                           dz_NOT_SYNC = 0;                        
#ifdef MQ2_SENSOR_USED
                           digitalWrite(Relay1, HIGH); delay(2000); // Run the siren
#endif
#ifdef KD5810_SENSOR_USED
                           digitalWrite(Relay2, HIGH); delay(3000); // Force KD5810 Smoke Detector siren to run 
                           // digitalWrite(Relay2, LOW);  delay(2000); 
                           cstate_KD5810 = digitalReadF(KD5810_IO_pin7,false);
                           Serial.print("KD5810_IO_pin7 = "); Serial.println(cstate_KD5810); 
#endif
                           Serial.println("Fire Alert from Domoticz. Smoke Detector already armed - Server state to FIRE");
                  }  // else if( FIREALARM_STATE == ARM )  { 
             } else if ( strcmp(cmde, "30") == 0 ) {  // SIREN/SIRENE
                  dz_FIREALARM_STATE = SIREN; 
                  if( FIREALARM_STATE == SIREN ) dz_NOT_SYNC = 0;
                  else if( FIREALARM_STATE == DISARM ) { 
                      FIREALARM_STATE = SIREN;  
                      dz_NOT_SYNC = 0;
#ifdef MQ2_SENSOR_USED
                      digitalWrite(Relay1, HIGH); delay(2000); // Run the siren
#endif
#ifdef KD5810_SENSOR_USED                      
                      digitalWrite(Relay1, HIGH); delay(2000); // Power up the KD5810 Smoke Detector
                      digitalWrite(Relay2, HIGH); delay(3000); // Force KD5810 Smoke Detector siren to run
                      // digitalWrite(Relay2, LOW);  delay(2000);
                      cstate_KD5810 = digitalReadF(KD5810_IO_pin7,false);
                      Serial.print("KD5810_IO_pin7 = "); Serial.println(cstate_KD5810); 
#endif
                      Serial.println("Other Alert than Fire from Domoticz. Smoke detector ARMED. Server state to SIREN");
                  } // if( FIREALARM_STATE == DISARM ) { 
                  else if( FIREALARM_STATE == ARM ) {   // If no alert already set, force siren to run 
                           FIREALARM_STATE = SIREN;
                           dz_NOT_SYNC = 0;
#ifdef MQ2_SENSOR_USED
                           digitalWrite(Relay1, HIGH); delay(2000); // Run the siren
#endif
#ifdef KD5810_SENSOR_USED
                           digitalWrite(Relay2, HIGH); delay(3000); // Force KD5810 Smoke Detector siren to run 
                           // digitalWrite(Relay2, LOW);  delay(2000); 
                           cstate_KD5810 = digitalReadF(KD5810_IO_pin7,false);
                           Serial.print("KD5810_IO_pin7 = "); Serial.println(cstate_KD5810); 
#endif
                           Serial.println("Other Alert than Fire from Domoticz. Smoke Detector already armed. Server state to SIREN");
                  } // else if( FIREALARM_STATE == ARM )                 
             } else if ( strcmp(cmde, "40") == 0 ) {    // TEST
                  dz_FIREALARM_STATE = TEST; 
                  if( FIREALARM_STATE == DISARM ) { 
#ifdef MQ2_SENSOR_USED
                      digitalWrite(Relay1, HIGH); delay(2000); // Check the siren is OK
                      digitalWrite(Relay1, LOW);  delay(2000);
                      // Start new calibration and send Ro value to Domoticz         
                      Ro = MQCalibration(MQ_PIN);                                        
                      FIREALARM_STATE = ARM;
#endif
#ifdef KD5810_SENSOR_USED 
                      digitalWrite(Relay1, HIGH); delay(2000); // Power up the KD5810 Smoke Detector                         
                      digitalWrite(Relay2, HIGH); delay(3000); // Enter Test Mode
                      digitalWrite(Relay2, LOW);  delay(2000);
                      cstate_KD5810 = digitalReadF(KD5810_IO_pin7,false);
                      Serial.print("Smoke detector in Test Mode. KD5810_IO_pin7 = "); Serial.println(cstate_KD5810); 
                      digitalWrite(Relay1, LOW);  delay(2000); // Power down the KD5810 Smoke Detector
                      if( cstate_KD5810 == KD5810_ALERT ) {
                          FIREALARM_STATE = ARM;
                          Serial.println("Smoke detector Test OK");
                          digitalWrite(Relay1, HIGH); delay(2000); // Power up the Smoke Detector
                      } else Serial.println("Smoke detector Test FAILED");
#endif
                  } // if( FIREALARM_STATE == DISARM ) { 
                  else if( FIREALARM_STATE == ARM ) {    
#ifdef MQ2_SENSOR_USED
                           digitalWrite(Relay1, HIGH); delay(2000); // Check the siren is OK
                           digitalWrite(Relay1, LOW);  delay(2000);
                           // Start new calibration and send Ro value to Domoticz                
                           Ro = MQCalibration(MQ_PIN);                                        
#endif
#ifdef KD5810_SENSOR_USED
                           digitalWrite(Relay2, HIGH); delay(3000); // Enter Test Mode
                           digitalWrite(Relay2, LOW);  delay(2000); 
                           cstate_KD5810 = digitalReadF(KD5810_IO_pin7,false);
                           Serial.print("Smoke detector in Test Mode. KD5810_IO_pin7 = "); Serial.println(cstate_KD5810); 
                           digitalWrite(Relay1, LOW);  delay(2000); // Reset the Smoke Detector
                           if( cstate_KD5810 == KD5810_NOALERT ) { FIREALARM_STATE = DISARM; Serial.println("Smoke detector Test FAILED"); }
                           else { Serial.println("Smoke detector Test OK"); digitalWrite(Relay1, HIGH); delay(2000); } // Power up again the Smoke Detector
#endif                           
                  } // else if( FIREALARM_STATE == ARM )                 
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
             string = "{\"command\" : \"addlogmessage\", \"message\" : \"iot_EDragon_FIREalarm Online - COLD BOOT Reason : " + ESP.getResetReason() + " - Firmware Version : 0.20 - SSID/IP/MAC : " + WiFi.SSID() + "/" + WiFi.localIP().toString() + "/" + WiFi.macAddress().c_str() + "\"}";
        else if ( WiFiWasRenew )
                  string = "{\"command\" : \"addlogmessage\", \"message\" : \"iot_EDragon_FIREalarm Online - WIFI CONNECTION RENEWED - SSID/IP/MAC : " + WiFi.SSID() + "/" + WiFi.localIP().toString() + "/" + WiFi.macAddress().c_str() + "\"}";   
             else string = "{\"command\" : \"addlogmessage\", \"message\" : \"iot_EDragon_FIREalarm Online - MQTT CONNECTION RENEWED - SSID/IP/MAC : " + WiFi.SSID() + "/" + WiFi.localIP().toString() + "/" + WiFi.macAddress().c_str() + "\"}";   
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
        Smoke = MQGetGasPercentage(MQRead(MQ_PIN)/Ro,GAS_SMOKE);
        if( FIREALARM_STATE == ARM  && ( Smoke >= Smoke_alert_threshold  ) ) { FIREALARM_STATE = FIRE; digitalWrite(Relay1, HIGH); delay(2000); Serial.println("SMOKE detected"); dz_NOT_SYNC = 1; }
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
  float _Smoke;

#ifdef MQ2_SENSOR_USED
  // MQGetGasPercentage(MQRead(MQ_PIN)/Ro,GAS_LPG) );
  // MQGetGasPercentage(MQRead(MQ_PIN)/Ro,GAS_CO) );
  _Smoke = MQGetGasPercentage(MQRead(MQ_PIN)/Ro,GAS_SMOKE);
  if( _Smoke > Smoke ) Smoke = _Smoke;
#endif 

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

       if( isnan(t) || isnan(h) ) {
           Serial.println("DHT22 Reading Error");
           if( TempSensorTimeOut > 0 )  {
               TempSensorTimeOut--;
               string = INDOOR_RDC_TEMP_ID;
               string = "{\"command\" : \"addlogmessage\", \"message\" : \"Error - " + string + " has returned a wrong value (NAN).\"}";
               string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
               Serial.print(msgToPublish);
               Serial.print(" Published to domoticz/in. Status=");
               if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK\n"); else Serial.println("KO\n"); 
           }  // if( TempSensorTimeOut > 0 )  {   
           else if( TempSensorTimeOut == 0 ) {
               TempSensorTimeOut--;
               string = "{\"command\" : \"switchlight\", \"idx\" : " + dz_TEMPFAILUREFLAG_IDX + ", \"switchcmd\" : \"On\", \"passcode\" : " + DZ_DEVICE_PASSWORD + "}"; 
               string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
               Serial.print(msgToPublish);
               Serial.print(" Published to domoticz/in. Status=");
               if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK\n"); else Serial.println("KO\n");            
           } // if( TempSensorTimeOut == 0 )  
       } // if( isnan(t) || isnan(h) ) {
       else {
           TempSensorTimeOut = 2 * (60 / DZ_SYNC_TIMER);
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

#ifdef MQ2_SENSOR_USED
       // Send to Domoticz the max smoke value for the last minute
       string = Smoke; 
       string = "{\"idx\" : " + dz_FIREALARM_SMOKE_IDX + ", \"nvalue\" : 0, \"svalue\" : \"" + string + "\"}"; 
       string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
       Serial.print(msgToPublish);
       Serial.print(" Published to domoticz/in. Status=");
       if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK\n"); else Serial.println("KO\n"); 
       Smoke = 0;  
#endif  
  } // if ( min > timer  ) { 

  // Check if we have a Fire Alert !
  if( FIREALARM_STATE == ARM || FIREALARM_STATE == FIRE ) {
#ifdef KD5810_SENSOR_USED
      cstate_KD5810 = digitalReadF(KD5810_IO_pin7,false);
      // Serial.print("KD5810_IO_pin7 = "); Serial.println(cstate_KD5810);      
      if( FIREALARM_STATE == ARM  && cstate_KD5810 == KD5810_ALERT )   { FIREALARM_STATE = FIRE; Serial.println("SMOKE detected");              dz_NOT_SYNC = 1; }
      if( FIREALARM_STATE == FIRE && cstate_KD5810 == KD5810_NOALERT ) { FIREALARM_STATE = ARM;  Serial.println("Back to NON SMOKE condition"); dz_NOT_SYNC = 1; }
#endif
#ifdef MQ2_SENSOR_USED
      if( FIREALARM_STATE == ARM  && ( _Smoke >= Smoke_alert_threshold  ) )   {
          FIREALARM_STATE = FIRE; digitalWrite(Relay1, HIGH); delay(2000);
          Serial.print("SMOKE detected - Smoke ppm/Raw_Adc = "); Serial.print(_Smoke); Serial.print("/"); Serial.println(MQ2_Raw_adc);
          dz_NOT_SYNC = 1;
      } // if( FIREALARM_STATE == ARM  && ( _Smoke >= Smoke_alert_threshold  ) 
      // if( FIREALARM_STATE == FIRE && ( _Smoke < 0.8*Smoke_alert_threshold ) ) { FIREALARM_STATE = ARM;  digitalWrite(Relay1, LOW);  delay(2000); Serial.println("Back to NON SMOKE condition"); dz_NOT_SYNC = 1; }
#endif
  }  // if( FIREALARM_STATE == ARM || FIREALARM_STATE == FIRE ) {
  
  // Log MQ-2 calibration result and alert threshold parameter
#ifdef MQ2_SENSOR_USED
  if( MQ2calibration ) {
      string = Ro; 
      string = "{\"command\" : \"addlogmessage\", \"message\" : \"iot_EDragon_FIREalarm - MQ-2 Ro/Smoke Alert Parameter = " + string + "/" + Smoke_alert_threshold + "\"}";
      string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
      Serial.print(msgToPublish);
      Serial.print("Published to domoticz/in. Status=");
      if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK"); else Serial.println("KO"); 
      MQ2calibration = false;
  }
#endif

  // Synchronize Domoticz if necessary
  if( dz_NOT_SYNC == 1 ) { 
      dz_NOT_SYNC = -1;
      string = "{\"command\" : \"switchlight\", \"idx\" : " + dz_FIREALARM_SWITCH_IDX + ", \"switchcmd\": \"Set Level\", \"level\" : " + FIREALARM_STATE + ", \"passcode\" : " + DZ_DEVICE_PASSWORD + "}";  
      string.toCharArray( msgToPublish, MQTT_MAX_PACKET_SIZE);
      Serial.print(msgToPublish);
      Serial.print(" Published to domoticz/in. Status=");
      if ( client.publish(topic_Domoticz_IN, msgToPublish) ) Serial.println("OK"); else Serial.println("KO");  
  } // if( dz_FIREALARM_STATE != FIREALARM_STATE ) {
  
  client.loop();
  delay(100);
} // void loop()  **************** 
/* THE END */

 
    


