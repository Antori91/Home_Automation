// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// ***** Script to poll ESP8266/DHT22 sensors every n minutes and then update Domoticz *****
// V0.4 - September 2017 

var httpDomoticz = require('http');
var JSON_API = {
host: 'localhost',         //[$$DOMOTICZ_PARAMETER]
port: 8084,                //[$$DOMOTICZ_PARAMETER]
path: '/'
};

const passwordProtectedDevice  = '$$YOUR_PASSWORD$$';   //[$$DOMOTICZ_PARAMETER]
const idxHeatingSelector       = '17';     //[$$DOMOTICZ_PARAMETER]
const idxTempFailureFlag       = '43';     //[$$DOMOTICZ_PARAMETER]

var DomoticzJsonTalk = function( JsonUrl, callBack, objectToCompute ) {    // Function to talk to DomoticZ. Do Callback if any 
   var savedURL=JSON.stringify(JsonUrl);
   // console.log("\n** DomoticZ URL request=" + savedURL );
   httpDomoticz.get(JsonUrl, function(resp){
      resp.on('data', function(ReturnData){ 
         // console.log("\nDomoticZ answer=" + ReturnData);
         if( callBack ) callBack( null, JSON.parse(ReturnData), objectToCompute );
      });
   }).on("error", function(e){
         // console.log("\n** Error - " + e.message + "\nCan't reach DomoticZ with URL:" + savedURL );     
         if( callBack ) callBack( e, null, objectToCompute );
   });
};   // function DomoticzJsonTalk( JsonUrl ) 

var RaisefailureFlag = function( error, data ) {
  if( error ) return;
  if( data.result[0].Status === "Off" ) {
       JSON_API.path = '/json.htm?type=command&param=switchlight&idx=' + idxTempFailureFlag + '&switchcmd=On&passcode=' + passwordProtectedDevice;
       JSON_API.path = JSON_API.path.replace(/ /g,"");
       DomoticzJsonTalk( JSON_API );                
  }  // if( GetAlarmIDX.result[0].Status === 
}; // var RaisefailureFlag = fu

var ESP8266DHT22 = require('http');
var ExternalSensor = {
Device: "DHT22",
ID: 'OUTDOOR_ESP8266_DHT22',
SensorUnavailable : 1,
NbReadError : 0,
host: '192.168.1.101',
port: 80,
path: '/',
lastTemp: -150,
lastHumidity: -50,
lastHumStat: '-50',
lastHeatIndex: -150,
idxTempHum: 8,            //[$$DOMOTICZ_PARAMETER]
idxHeatIndex: 15          //[$$DOMOTICZ_PARAMETER]
};
var InternalSensor = {
Device: "DHT22",
ID: 'INDOOR_ESP8266_DHT22',
SensorUnavailable : 1,
NbReadError : 0,
host: '192.168.1.102',
port: 80,
path: '/',
lastTemp: -150,
lastHumidity: -50,
lastHumStat: '-50',
lastHeatIndex: -150,
idxTempHum: 9,             //[$$DOMOTICZ_PARAMETER]
idxHeatIndex: 14           //[$$DOMOTICZ_PARAMETER]
};
var DegreesDaysSensor = {          // Virtual device !
Device: "DEGREESDAYS",
ID: 'DEGREESDAYS',
SensorUnavailable : 1,
lastTemp: -150,
lastHumidity: -50,
lastHumStat: '-50',
idxTempHum: 25,            //[$$DOMOTICZ_PARAMETER]
idxHeatIndex: -1,      // -1 means no HeatIndex for this sensor
ComputeDegreesDays: function( error, data, DegreesDaysValue ) {
  // Compute Degrees-days with BaseTemp=18°C
  // TEMP=Degrés-jour = (18°C - ExternalTemp) 
  // HUM=Degrés-jour_Chauffage_Effectif% = 100 * ( (InternalTemp -1°C (apport occupant) - ExternalTemp) / (18°C - ExternalTemp) )
  // Heating level 0/10/20/30 for OFF/HorsGel/Eco/Confort. If OFF or HorsGel, we assume Heating is Power Off (i.e Degrés-jour_Chauffage_Effectif%=0)
  DegreesDaysValue.lastTemp=0; DegreesDaysValue.lastHumidity=100; DegreesDaysValue.lastHumStat='0';
  DegreesDaysValue.SensorUnavailable = ExternalSensor.SensorUnavailable;
  if ( !DegreesDaysValue.SensorUnavailable && ExternalSensor.lastTemp < 18 ) { DegreesDaysValue.lastTemp=(18-ExternalSensor.lastTemp); DegreesDaysValue.lastTemp=DegreesDaysValue.lastTemp.toFixed(1); }
  if( error ) return;
  if ( data.result[0].Level === 0 || data.result[0].Level === 10 ) DegreesDaysValue.lastHumidity=0; 
      else if ( !DegreesDaysValue.SensorUnavailable && ExternalSensor.lastTemp < 18 && !InternalSensor.SensorUnavailable ) { 
                  DegreesDaysValue.lastHumidity=100*(InternalSensor.lastTemp-1-ExternalSensor.lastTemp)/(18-ExternalSensor.lastTemp); DegreesDaysValue.lastHumidity = DegreesDaysValue.lastHumidity.toFixed(1); 
      } 
  if (DegreesDaysValue.lastHumidity <= 50) DegreesDaysValue.lastHumStat='2'; else if (DegreesDaysValue.lastHumidity > 100) DegreesDaysValue.lastHumStat='1';         
  }  // var ComputeDegreesDays
};
// ** Temp sensors list. 
var MyTempSensors = [ExternalSensor, InternalSensor, DegreesDaysSensor];  
const PollingTimer  = 5; // Poll the sensors every n minutes
const SensorTimeOut = 2 * (60/PollingTimer); // If a Sensor is not available for n hours, raise TEMP SENSOR ALARM in DomoticZ
var lastDaySeen     = new Date().getDate();

// ***** INIT
console.log("*** " + new Date() + " - Domoticz iot_ESP8266/DHT22 started ***\n");

// ***** LOOP à la Arduino mode...
setInterval(function(){ // get Temp/Humidity from sensors every n minutes
   
    // *** Print each day, the total number of DHT22 reading errors
    var Today = new Date().getDate();
    if( Today !== lastDaySeen ) {
       console.log("\n*** " + new Date() + " Indoor/Outdoor DHT22 NAN Reading Error=", InternalSensor.NbReadError, "/", ExternalSensor.NbReadError);
       InternalSensor.NbReadError =  ExternalSensor.NbReadError = 0;
       lastDaySeen=Today;
    }

    // *** READ SENSORS LOOP  *** 
    MyTempSensors.forEach(function(value) {
           var ERROR_MESSAGE = "";
           if( value.Device === "DEGREESDAYS" )   {
               JSON_API.path = '/json.htm?type=devices&rid=' + idxHeatingSelector;
               DomoticzJsonTalk( JSON_API, value.ComputeDegreesDays, value );                            
           } else  // if( value.Device === "DEGREESDAYS" )   
               ESP8266DHT22.get(value, function(resp){
                  resp.on('data', function(JSONSensor) {
                     // console.log("Sensor=" + JSONSensor);
                     const CurrentSensorValue = JSON.parse(JSONSensor);                           
                     if( CurrentSensorValue.Temperature_Celsius === "DHT22 ERROR" ) {     // ESP8266 available but a DHT22 reading error occured 
                         value.SensorUnavailable++;  value.NbReadError++;
                         ERROR_MESSAGE="Error - " + value.ID + " has returned a wrong value (NAN)";   
                         ERROR_MESSAGE=ERROR_MESSAGE.replace(/ /g,"%20");
                         JSON_API.path = '/json.htm?type=command&param=addlogmessage&message=' + ERROR_MESSAGE;
                         DomoticzJsonTalk( JSON_API );  
                     } else { 
                         value.lastTemp=CurrentSensorValue.Temperature_Celsius;
                         value.lastHumidity=CurrentSensorValue.Humidity_Percentage;
                         value.lastHeatIndex=CurrentSensorValue.HeatIndex_Celsius;
                         value.lastHumStat='0';
                         if (value.lastHumidity <= 30 ) value.lastHumStat='2'
                            else if (value.lastHumidity >= 70 ) value.lastHumStat='3'
                                 else if (value.lastHumidity >= 45 && value.lastHumidity <= 65 ) value.lastHumStat='1';
                         value.SensorUnavailable = 0;              
                     }  // // if( CurrentSensorValue.Temperature_Celsius == "DHT22 ERROR" )                    
                  }); //resp.on('data', function(JSONSensor
               }).on("error", function(e){                  
                     if( value.SensorUnavailable===1) { // Log this error in DomoticZ only the first time in a row
                         ERROR_MESSAGE="Error - " + value.ID + " " + e.message;   
                         ERROR_MESSAGE=ERROR_MESSAGE.replace(/ /g,"%20");
                         JSON_API.path = '/json.htm?type=command&param=addlogmessage&message=' + ERROR_MESSAGE;
                         DomoticzJsonTalk( JSON_API );
                     }  // if( value.SensorUnavailable===1  
                     value.SensorUnavailable++;  value.NbReadError++;
               }); // ESP8266DHT22.get(value, function(resp){   
    }); // MyTempSensors.forEach(f
     
    //  *** DOMOTICZ UPDATE LOOP  ***
    MyTempSensors.forEach(function(value) {      
           // Log temperature and humidity     
           if( !value.SensorUnavailable )  {
               JSON_API.path = '/json.htm?type=command&param=udevice&idx='+ value.idxTempHum +'&nvalue=0&svalue=' + value.lastTemp + ';' + value.lastHumidity + ';' + value.lastHumStat;
               JSON_API.path = JSON_API.path.replace(/ /g,"");
               DomoticzJsonTalk( JSON_API );  
               // Log HeatIndex temperature  
               if( value.idxHeatIndex !== -1 ) {
                   JSON_API.path = '/json.htm?type=command&param=udevice&idx='+ value.idxHeatIndex +'&nvalue=0&svalue=' + value.lastHeatIndex;
                   JSON_API.path = JSON_API.path.replace(/ /g,"");
                   DomoticzJsonTalk( JSON_API );
               }  // if( value.idxHeatIndex !== -1 ) {
           }  // if( !value.SensorUnavailable )  {
    }); // MyTempSensors.forEach(f
    
    // *** SENSORS HEALTH CHECK LOOP. If we have a sensor failure for n hours, raise TEMP SENSOR FAILURE in DomoticZ *** 
    MyTempSensors.forEach(function(value) {      
           if( value.SensorUnavailable >= SensorTimeOut ) {
               JSON_API.path = '/json.htm?type=devices&rid=' + idxTempFailureFlag;
               DomoticzJsonTalk( JSON_API, RaisefailureFlag );               
           }  //  if( value.SensorUnavaila
    }); // TempSensors.forEach(f
    
}, PollingTimer*60000); // setInterval(function(){  get Temp/Humidity from sensors every n mn