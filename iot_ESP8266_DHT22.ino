// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// ***** Script to read DHTxx sensor and send the measure as HTTP JSON message *****

#include "DHT.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>


const char *ssid = "$$YOUR_SSID$$";
const char *password = "$$YOUR_WIFI_PASSWORD$$";
IPAddress ip(192,168,1,101); 
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
ESP8266WebServer server ( 80 );

#define SID "$$YOUR_SENSOR_ID$$"
#define JSONstart "{"
#define JSONend "}"

#define DHTPIN 5    // what digital pin we're connected to
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
DHT dht(DHTPIN, DHTTYPE);

void handleRoot() {
	char temp[520];
  char str_t[10];
  char str_h[10];
  char str_hic[10];
 
	int sec = millis() / 1000;
	int min = sec / 60;
	int hr = min / 60;

  // Wait a few seconds between measurements.
  delay(2000);

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  
  /* Check if any reads failed and exit early (to try again) */
  if ( isnan(h) || isnan(t) ) {
     /* Serial.println("Failed to read from DHT sensor!"); */
     snprintf ( temp, 520,
"%s\n\
\"SensorID\":\"%s\",\n\
\"Temperature_Celsius\":\"%s\",\n\
\"Humidity_Percentage\":\"%s\",\n\
\"HeatIndex_Celsius\":\"%s\",\n\
\"Sensor_Elapse_Running_Time\":\"%02d:%02d:%02d\"\n\
%s",
     JSONstart, SID, "DHT22 ERROR", "DHT22 ERROR", "DHT22 ERROR", hr, min % 60, sec % 60, JSONend
     );
  } // if ( isnan(h) || isnan(t) ) {
  else {
     /* Compute heat index in Celsius (isFahreheit = false) */
     float hic = dht.computeHeatIndex(t, h, false);
     dtostrf(t, 5, 1, str_t);
     dtostrf(h, 5, 1, str_h);
     dtostrf(hic, 5, 1, str_hic);
   	 snprintf ( temp, 520,
"%s\n\
\"SensorID\":\"%s\",\n\
\"Temperature_Celsius\":\"%s\",\n\
\"Humidity_Percentage\":\"%s\",\n\
\"HeatIndex_Celsius\":\"%s\",\n\
\"Sensor_Elapse_Running_Time\":\"%02d:%02d:%02d\"\n\
%s",
     JSONstart, SID, str_t, str_h, str_hic, hr, min % 60, sec % 60, JSONend
     );
  } /* else if if (isnan(h) || isnan(t) || isnan(f)) */
	
	server.send ( 200, "application/json", temp );
} /* void handleRoot() { */

void handleNotFound() {
	String message = "URL Not Found\n\n";
	message += "URI: ";
	message += server.uri();
	message += "\nMethod: ";
	message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
	message += "\nArguments: ";
	message += server.args();
	message += "\n";

	for ( uint8_t i = 0; i < server.args(); i++ ) {
		message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
	}
	server.send ( 404, "text/plain", message );
} /* handleNotFound() { */

void setup ( void ) {

  dht.begin();
	Serial.begin ( 57600 );
	WiFi.begin ( ssid, password );
  WiFi.config(ip, gateway, subnet);
	Serial.println ( "" );

	// Wait for connection
	while ( WiFi.status() != WL_CONNECTED ) {
		delay ( 500 );
		Serial.print ( "." );
	}

	Serial.println ( "" );
	Serial.print ( "Connected to " );
	Serial.println ( ssid );
	Serial.print ( "IP address: " );
	Serial.println ( WiFi.localIP() );

	if ( MDNS.begin ( "esp8266" ) ) {
		Serial.println ( "MDNS responder started" );
	}

	server.on ( "/", handleRoot );
	server.on ( "/inline", []() {
		server.send ( 200, "text/plain", "$$YOUR_SENSOR_ID$$.Temp/RESTAPI Reserved Use Mode" );
	} );
	server.onNotFound ( handleNotFound );
	server.begin();
	Serial.println ( "HTTP server started" );
}

void loop ( void ) {
	server.handleClient();
}