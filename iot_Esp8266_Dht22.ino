// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// ***** Temperature sensor using DHT22  *****
// V0.26 - June 2018
          // IMPROVEMENT : Further logging (when using Rob Tillaart library) about DHT22 reading errors  
// V0.2  - June 2018 
          // Rob Tillaart library supported in addition to Adafruit one to interface the DHT22 sensor
          // Wifi mode set to STA only
          // OTA available
          // Flash parameters for ANTORI Board (ESP8266 12-E) 
          // changed from "Generic ESP8266 Module, CPU Freq 80 MHz, Flash Freq 40 MHz, Flash mode DIO, Upload speed 57600,  Flash size 1M (512K SPIFFS), ck, Disabled, None"
          // to           "Generic ESP8266 Module" CPU Freq 80 Mhz, Flash Freq 40 Mhz, Flash mode QIO, Upload Speed 115200, FlashSize= 1M (64K SPIFFS),  ck, Disabled, None"                   
// V0.11 - March 2017  
          // Initial release 
          
#define   ADAFRUIT_LIB  true
// #define   ROBTILLAART_LIB true
// #define   LOOPBACK_TEST_LIB  true
#define   ANTORI_BOARD    true 
#define   FIXED_IP_USED   true
#include  "WiFi_OTA_MQTT_SecretKeys.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include  <ArduinoOTA.h>
#define   OTA_HOSTNAME_MAXLENGTH 15

// WiFi parameters
#define   NUMBER_OF_AP                    4
byte      WiFi_AP                       = 1; // The WiFi Access Point we are connected to 
const     char *ssid[NUMBER_OF_AP]      = { WIFI_1,     WIFI_2,     WIFI_3,     WIFI_4 };
const     char *password[NUMBER_OF_AP]  = { WIFI_1_PWD, WIFI_2_PWD, WIFI_3_PWD, WIFI_4_PWD };
#define   MAC_LENGTH                      6
byte      mac[MAC_LENGTH];
#define   MAC_UNKNOWN                     "FFFFFF"
#define   WiFiRetryDelay                  2000 // Delay in ms between each WiFi connection retry

// Temp sensor parameters
#ifdef    ADAFRUIT_LIB
#include "DHT.h"
#endif
#ifdef    ROBTILLAART_LIB
#include  <dht.h>
#endif
#ifdef    ANTORI_BOARD
#define   DHTPIN 5         // Digital pin connected to DHT22
#endif
#ifdef    EDRAGON_BOARD
#define   DHTPIN                                     14
#endif
#define   DHTTYPE DHT22
#ifdef    ADAFRUIT_LIB
DHT       dht(DHTPIN, DHTTYPE);
#endif
#ifdef    ROBTILLAART_LIB
dht       DHT;
#endif
#define   NBR_TEMP_SENSORS                           2
int       thisTEMP_SENSOR=0; 
String    TEMP_SENSOR_MAC6[NBR_TEMP_SENSORS+1]     = { " 6DD 5", " 4F62D", MAC_UNKNOWN };   
IPAddress ip[NBR_TEMP_SENSORS+1]                   = { OUTDOOR_TEMP_IP, INDOOR_TEMP_IP, NEW_TEMP_IP }; 
String    TEMP_SENSOR_ID[NBR_TEMP_SENSORS+1]       = { OUTDOOR_TEMP_ID, INDOOR_TEMP_ID, NEW_TEMP_ID };                                                        
byte      TEMP_SENSOR_WIFI_AP[NBR_TEMP_SENSORS+1]  = { 1, 1, 1 };                                                                                         
String    TEMP_SENSOR_MAC                          = "";

// Web server parameters
ESP8266WebServer server( 80 );    
#define   HTTP_MAX_ANSWER_SIZE 512  
char      HTTPAnswer[HTTP_MAX_ANSWER_SIZE+1]; 


#ifdef ROBTILLAART_LIB
float convertCtoF(float c) { return c * 1.8 + 32; }
float convertFtoC(float f) { return (f - 32) * 0.55555; }
float computeHeatIndex(float temperature, float percentHumidity, bool isFahrenheit) {
  //boolean isFahrenheit: True == Fahrenheit; False == Celcius
  // Using both Rothfusz and Steadman's equations
  // http://www.wpc.ncep.noaa.gov/html/heatindex_equation.shtml
  float hi;
  if (!isFahrenheit)
    temperature = convertCtoF(temperature);
  hi = 0.5 * (temperature + 61.0 + ((temperature - 68.0) * 1.2) + (percentHumidity * 0.094));
  if (hi > 79) {
    hi = -42.379 +
             2.04901523 * temperature +
            10.14333127 * percentHumidity +
            -0.22475541 * temperature*percentHumidity +
            -0.00683783 * pow(temperature, 2) +
            -0.05481717 * pow(percentHumidity, 2) +
             0.00122874 * pow(temperature, 2) * percentHumidity +
             0.00085282 * temperature*pow(percentHumidity, 2) +
            -0.00000199 * pow(temperature, 2) * pow(percentHumidity, 2);
    if((percentHumidity < 13) && (temperature >= 80.0) && (temperature <= 112.0))
      hi -= ((13.0 - percentHumidity) * 0.25) * sqrt((17.0 - abs(temperature - 95.0)) * 0.05882);
    else if((percentHumidity > 85.0) && (temperature >= 80.0) && (temperature <= 87.0))
      hi += ((percentHumidity - 85.0) * 0.1) * ((87.0 - temperature) * 0.2);
  }
  return isFahrenheit ? hi : convertFtoC(hi);
} // float computeHeatIndex(float temperature, float percentHumidity, bool isFahrenheit)
#endif

#ifdef LOOPBACK_TEST_LIB
#define FILTER 3 // N+1 same readings every DELAY_FILTER ms must be read to validate GPIO reading ! 
#define DELAY_FILTER 100 // Delay in ms between two readings 
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
#endif

void handleRoot() {
  String answer;
  char str_t[25];
  char str_h[25];
  char str_hic[25];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
  // Serial.print("\n== New client request, DHT sensor status=");
  
#ifdef ROBTILLAART_LIB  
  int chk = DHT.read22(DHTPIN);
  switch (chk)
  {
    case DHTLIB_OK:
        dtostrf( DHT.temperature, 5, 1, str_t );
        dtostrf( DHT.humidity,    5, 1, str_h );
        dtostrf( computeHeatIndex(DHT.temperature, DHT.humidity, false), 5, 1, str_hic );
        // Serial.print("OK, ");
        break;
    case DHTLIB_ERROR_CHECKSUM:
        strcpy( str_t, "DHT22 ERROR"); strcpy( str_h, "DHT22 CHECKSUM ERROR"); strcpy( str_hic, "DHT22 CHECKSUM ERROR");
        // Serial.print("Checksum error, ");
        break;
    case DHTLIB_ERROR_TIMEOUT:
        strcpy( str_t, "DHT22 ERROR"); strcpy( str_h, "DHT22 TIMEOUT ERROR"); strcpy( str_hic, "DHT22 TIMEOUT ERROR");
        // Serial.print("Time out error, ");
        break;
    default:
        strcpy( str_t, "DHT22 ERROR"); strcpy( str_h, "DHT22 UNKNOWN ERROR"); strcpy( str_hic, "DHT22 UNKNOWN ERROR");
        // Serial.print("Unknown error, ");
        break;
  } // switch (chk)
#endif 
    
#ifdef ADAFRUIT_LIB
  float t = dht.readTemperature(); 
  float h = dht.readHumidity();
  float hic;
  if ( isnan(t) )             strcpy( str_t,   "DHT22 ERROR");   else dtostrf(t, 5, 1, str_t);  
  if ( isnan(h) )             strcpy( str_h,   "DHT22 UNKNOWN ERROR");   else dtostrf(h, 5, 1, str_h); 
  if ( isnan(t) || isnan(h) ) strcpy( str_hic, "DHT22 UNKNOWN ERROR");   else { hic = dht.computeHeatIndex(t, h, false); dtostrf(hic, 5, 1, str_hic); }
#endif 

#ifdef LOOPBACK_TEST_LIB
  strcpy( str_t, "DHT22 ERROR");
  strcpy( str_h, "GPIO LOOPBACK TEST"); 
  dtostrf(  digitalReadF( DHTPIN ), 5, 1, str_hic );
#endif
  
  answer = "{\n\"SensorID\":\"" + TEMP_SENSOR_ID[thisTEMP_SENSOR] + "\",\n\"Temperature_Celsius\":\"" + str_t + "\",\n\"Humidity_Percentage\":\"" + str_h + "\",\n\"HeatIndex_Celsius\":\"" + str_hic  +
              "\",\n\"Sensor_Elapse_Running_Time\":\"" + hr + ":" + min % 60 + ":" + sec % 60 + "\"\n}";   
  answer.toCharArray( HTTPAnswer, HTTP_MAX_ANSWER_SIZE);
  // Serial.print("HTTP message returned=");
  // Serial.println(HTTPAnswer);
  server.send ( 200, "application/json", HTTPAnswer );
  delay(2000);
} /* void handleRoot() { */

void setup ( void ) {

#ifdef ADAFRUIT_LIB
  dht.begin();    // Set DHT items 
#endif

#ifdef LOOPBACK_TEST_LIB
  pinMode(DHTPIN,INPUT); 
#endif

	Serial.begin (115200 );
  Serial.println("iot_ESP8266_DHT22 Booting - Firmware Version : 0.26");
   
  Serial.println("\nESP8266 hardware configuration :");
  Serial.print("ChipId: ");                   Serial.print( ESP.getChipId() );
  Serial.print(" - SdkVersion: ");            Serial.println( ESP.getSdkVersion() );
  Serial.print("CpuFreqMHz: ");               Serial.print( ESP.getCpuFreqMHz() );
  Serial.print(" - FlashChipSpeed: ");        Serial.println( ESP.getFlashChipSpeed() );
  Serial.print("FlashChipId: ");              Serial.print( ESP.getFlashChipId() );
  Serial.print(" - FlashChipMode: ");         Serial.print( ESP.getFlashChipMode() );
  Serial.print(" - FlashChipSizeByChipId: "); Serial.print( ESP.getFlashChipSizeByChipId() );
  Serial.print(" - FlashChipRealSize: ");     Serial.print( ESP.getFlashChipRealSize() );
  Serial.print(" - FlashChipSize: ");         Serial.println( ESP.getFlashChipSize() );
  Serial.println();
   
  // Find configuration main index 
  WiFi.macAddress(mac);
  for (byte i = 3; i < MAC_LENGTH; ++i) {
     char buf[3];
     sprintf(buf, "%2X", mac[i]);
     TEMP_SENSOR_MAC += buf;
  } // for (byte i = 3; i < MAC_LENGTH; ++i) {
  for( thisTEMP_SENSOR = 0; thisTEMP_SENSOR < NBR_TEMP_SENSORS;  ) {
      if ( TEMP_SENSOR_MAC6[thisTEMP_SENSOR] == TEMP_SENSOR_MAC ) break;
      else thisTEMP_SENSOR++;  
  }

  WiFi.mode(WIFI_STA);
  Serial.printf( "Trying to connect to preferred WiFi AP %s\n", ssid[WiFi_AP] );
  WiFi.begin(ssid[WiFi_AP], password[WiFi_AP]); 
  WiFi.config(ip[thisTEMP_SENSOR], gateway, subnet); 
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      if( ++WiFi_AP == NUMBER_OF_AP ) WiFi_AP = 0;
	    delay( WiFiRetryDelay ); 
      Serial.printf( "Trying to connect to WiFi AP %s\n", ssid[WiFi_AP] );
      WiFi.begin(ssid[WiFi_AP], password[WiFi_AP]);
      WiFi.config(ip[thisTEMP_SENSOR], gateway, subnet); 
  } // while (WiFi.waitForConnectResult() != WL_CONNECTED) {
  
  // Start OTA with port defaults to 8266
  // ArduinoOTA.setPort(8266);
  // Set OTA Hostname
  char bufhn[OTA_HOSTNAME_MAXLENGTH+1];
  TEMP_SENSOR_ID[thisTEMP_SENSOR].toCharArray(bufhn, OTA_HOSTNAME_MAXLENGTH+1);
  ArduinoOTA.setHostname(bufhn); 
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
   
	server.on ( "/", handleRoot );
	server.begin();
  
  // say we are now ready and give configuration items
  Serial.println( "Ready" );
  Serial.print  ( "Connected to " );
  Serial.printf ( "%s\n", ssid[WiFi_AP] );
  Serial.print  ( "IP address: " );
  Serial.println( WiFi.localIP() );  
  Serial.print  ( "Mac address: " );
  Serial.println( TEMP_SENSOR_MAC );
  Serial.print  ( "TEMP_SENSOR_ID: ");
  Serial.println( TEMP_SENSOR_ID[thisTEMP_SENSOR] ); 
}

void loop ( void ) {
  ArduinoOTA.handle();
	server.handleClient();
}




