// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// ***** Script to poll ESP8266/DHT22 http sensors every n minutes
//       compute DJU (Degree.Days) 
//       compute overall heating usage (from heaters ACS712 data), update heating forecast and house thermal characteristics (thermal loss and cooling rate)  *****
// V0.60 - January 2021
            // iot_AC712.js merged  
// V0.50 - December 2020                                                                 
            // IMPROVEMENT : Incremental counter used as DJU Domoticz device 
            // CHANGE : Read Temp sensors every minute (PollingTimer=1) and update Domoticz immediately 
            // Improvement : increase stability against communication and security issues
// V0.45 - June 2018
            // IMPROVEMENT : Further logging about DHT22 reading errors   
// V0.4 - September 2017 
            // ** NEW FEATURE : If any Temp sensor failure, raise TEMP SENSOR ALARM IDX in DomoticZ
            // New update loop with Array of objects for sensors
// V0.2d - Mars 2017 
            // ** NEW FEATURE : DJU computation. 
            // - Maxima/minima IDX removed
// V0.1  - Mars 2017 
            // Initial release 

const VERBOSE = false; // Detailed logging or not 
if( process.argv[ 2 ] === 'RASPBERRY')                                                         // Unless otherwise specified 
      MyJSecretKeys  = require('/home/pi/iot_domoticz/WiFi_DZ_MQTT_SecretKeys.js');            
else  MyJSecretKeys  = require('/volume1/@appstore/iot_domoticz/WiFi_DZ_MQTT_SecretKeys.js');  // execution hardware assumed to be Synology

// House thermal characteristics  
const House_H_Ratio            = 262;   // H ratio used for now (house thermal loss rate)= U*Surface -- Unit W/K or W/°C 
const HouseInternalHeatGain    = 1;     // Auto heating from house residents and domestic appliances -- Unit °C
var ElecTariff;                         // Electricity supplier tariff to use depending on day period : HP (Heures pleines) or HC (Heures creuses)
var HeatingSelector            = 0;     // Current heating mode selector (0/10/20/30 for OFF/HorsGel/Eco/Confort)
var Thermostat_setPoint        = null;  // Initialized to null to indicate we are at the first run

// Temp items
const PollingTimer             = 1;     // Poll the sensors every n minutes
const SensorTimeOut            = 2 * (60/PollingTimer); // If a Sensor is not available for n hours, raise TEMP SENSOR ALARM in DomoticZ
const r_Ratio_subset           = (4 * 60/PollingTimer); // 4 hours subset moving average
const OUTDOOR_TEMP_REACHED     = 0.5;   
// const INDOOR_TEMP_REACHED   = 3;
const THRESHOLD_DEGREESDAYS    = 0.050; // Domoticz incremental counter device updated every THRESHOLD_DEGREESDAYS tick 
var DayDJU                     = 0;     // Domoticz Current Total DJU of current day

// DomoticZ devices Idx
const passwordProtectedDevice  = MyJSecretKeys.ProtectedDevicePassword; 
const idxHeatingSelector       = MyJSecretKeys.idx_HeatingSelector;
const idxTempFailureFlag       = MyJSecretKeys.idx_TempFailureFlag;
const InternalTempSensor_IP    = MyJSecretKeys.INDOOR_ESP8266_DHT22;
const ExternalTempSensor_IP    = MyJSecretKeys.OUTDOOR_ESP8266_DHT22;
const TempSensor_Port          = MyJSecretKeys.TEMP_ESP8266_DHT22_PORT;
const idxIndoorTempHum         = MyJSecretKeys.idx_IndoorTempHum;
const idxIndoorHeatidx         = MyJSecretKeys.idx_IndoorHeatidx;
const idxOutdoorTempHum        = MyJSecretKeys.idx_OutdoorTempHum;
const idxOutdoorHeatidx        = MyJSecretKeys.idx_OutdoorHeatidx;
const idxElecTariff            = MyJSecretKeys.idx_ElecTariff;
const idxThermostat_setPoint   = MyJSecretKeys.idx_Thermostat_setPoint;
const idxThermalLoss           = MyJSecretKeys.idx_ThermalLoss;
const idxCoolingRate           = MyJSecretKeys.idx_CoolingRate;
const idxDJUTemp               = MyJSecretKeys.idx_DJUTemp;


var http_Domoticz = require('http');
var JSON_API = {
host: 'localhost',
port: MyJSecretKeys.DZ_PORT,
path: '/'
};

var DomoticzJsonTalk = function( JsonUrl, callBack, objectToCompute ) {    
   var savedURL         = JSON.stringify(JsonUrl);                     
   var _JsonUrl         = JSON.parse(savedURL);      // Function scope to capture values of JsonUrl and objectToCompute next line 
   var _objectToCompute = "";
   if( objectToCompute ) _objectToCompute = JSON.parse(JSON.stringify(objectToCompute));
   if( VERBOSE ) console.log("\n** DomoticZ URL request=" + savedURL );
   http_Domoticz.get(_JsonUrl, function(resp){
      var HttpAnswer = "";
      resp.on('data', function(ReturnData){ 
            HttpAnswer += ReturnData;
      });
      resp.on('end', function(ReturnData){ 
         if( VERBOSE ) console.log("\nDomoticZ answer=" + HttpAnswer);
         try { // To avoid crash if Domoticz returns a non JSON answer like <html><head><title>Unauthorized</title></head><body><h1>401 Unauthorized</h1></body></html>
               if( callBack ) callBack( null, JSON.parse(HttpAnswer), _objectToCompute );
         } catch (err) {
               if( VERBOSE ) console.log("\n** Error - " + err.message + "\nError to parse DomoticZ answer to request URL:" + savedURL );
               callBack( err, null, _objectToCompute );
         }  // try { // To avoid crash if D       
      });
   }).on("error", function(e){
         if( VERBOSE ) console.log("\n** Error - " + e.message + "\nCan't reach DomoticZ with URL:" + savedURL );
         if( callBack ) callBack( e, null, _objectToCompute );
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
Device            : "DHT22", // Device type (DHT22 sensor or DegreesDays virtual device)
ID                : 'OUTDOOR_ESP8266_DHT22',
SensorUnavailable : 1,       // 0= Sensor available, 1..= number of times sensor found unavailable
NbReadError       : 0,
host              : ExternalTempSensor_IP,
port              : TempSensor_Port,
path              : '/',
lastTemp          : null,    // latest reading
previousTemp      : null,    // reading before the latest
lastHumidity      : null,
lastHumStat       : null,
lastHeatIndex     : null,
idxTempHum        : idxOutdoorTempHum,
idxHeatIndex      : idxOutdoorHeatidx 
}; // var ExternalSensor = {

var InternalSensor = {
Device            : "DHT22",
ID                : 'INDOOR_ESP8266_DHT22',
SensorUnavailable : 1,
NbReadError       : 0,
host              : InternalTempSensor_IP,
port              : TempSensor_Port,
path              : '/',
lastTemp          : null,
previousTemp      : null,
lastHumidity      : null,
lastHumStat       : null,
lastHeatIndex     : null,
idxTempHum        : idxIndoorTempHum,          
idxHeatIndex      : idxIndoorHeatidx          
};  // var InternalSensor = {


var DegreesDaysSensor = {      
Device            : "DEGREESDAYS",
ID                : 'DEGREESDAYS',
lastTemp          : 0,
idxTemp           : idxDJUTemp,           
Compute: function() {
    // Compute additional Degree Days with BaseTemp=18°C and update Domoticz every THRESHOLD_DEGREESDAYS tick
    if ( !ExternalSensor.SensorUnavailable && ExternalSensor.lastTemp < 18 ) DegreesDaysSensor.lastTemp += (18-ExternalSensor.lastTemp)*(PollingTimer/1440);
    var DegDaysToAddTickCounter = Math.floor(DegreesDaysSensor.lastTemp/THRESHOLD_DEGREESDAYS); // Number of ticks to add to Domoticz  
    if( DegDaysToAddTickCounter > 0 ) {
        var DegDaysToAdd = (DegDaysToAddTickCounter * THRESHOLD_DEGREESDAYS);
        JSON_API.path = '/json.htm?type=command&param=udevice&idx='+ DegreesDaysSensor.idxTemp +'&nvalue=0&svalue=' + DegDaysToAdd.toFixed(3);
        DomoticzJsonTalk( JSON_API, function( error, data, DegDaysAdded ) {
            if ( error ) return;  // An error occured. Domoticz incremental counter device was not updated.
            DegreesDaysSensor.lastTemp -= DegDaysAdded;   
            DayDJU += DegDaysAdded;
            if( VERBOSE ) console.log("\n" + new Date() + " - DomoticZ DJU Update/Next DJU Update= " + DegDaysAdded + "/" + DegreesDaysSensor.lastTemp + " - DomoticZ Total DJU= " +  DayDJU + "\n");
        }, DegDaysToAdd  );   
    } // if( DegDaysToAddTickCounter > 0 ) {   
    else if( VERBOSE ) console.log("\n" + new Date() + " - Next DomoticZ DJU Update= " + DegreesDaysSensor.lastTemp + " - DomoticZ Total DJU= " +  DayDJU); 
  }  // Compute: function() {
}; // var DegreesDaysSensor = {  


var HeatingMeter = {
Device              : "METER",
ID                  : 'HEATING_METER',
SensorUnavailable   : 1,
IDX_subMeters       : ['27', '28', '29', '30', '31', '35', '36', '37', '38'], 
IDX_Meter           : 33,
MeterUsagePower     : 0, // Transient value
MeterPreviousPower  : 0, // Latest stable value (usage)
MeterReturnPower    : 0, // Transient value
MeterUsage1         : 0, // Transient value
MeterUsage2         : 0, // Transient value
MeterPreviousUsage1 : 0, // Latest stable value
MeterPreviousUsage2 : 0, // Latest stable value
MeterReturn1        : 0, // Used as Forecast (stable value)
MeterReturn2        : 0, // Used as Forecast (stable value)
NbSubMeterToRead    : 0,
UpdateMeter: function() {  

    if( HeatingMeter.SensorUnavailable ) return;
         
    // Get Elec tariff, Heating mode selector and Heating thermostat setpoint
    JSON_API.path = '/json.htm?type=devices&rid=' + idxElecTariff;
    DomoticzJsonTalk( JSON_API, function( error, data ) {
       ElecTariff = "HP";
       if( error ) return;
       if( data.result[0].Data === "Off") ElecTariff= "HC";
       if( VERBOSE ) console.log("Electricity Tariff= " + ElecTariff);
    }); // DomoticzJsonTalk( JSON_API, func
          
    JSON_API.path = '/json.htm?type=devices&rid=' + idxHeatingSelector;
    DomoticzJsonTalk( JSON_API, function( error, data ) {
       if( error ) return;
       HeatingSelector = data.result[0].Level;
       if( VERBOSE ) console.log("Heating Selector= " + HeatingSelector);
    });  // DomoticzJsonTalk( JSON_API, func
    
    JSON_API.path = '/json.htm?type=devices&rid=' + idxThermostat_setPoint;
    DomoticzJsonTalk( JSON_API, function( error, data ) {
       if( error ) return;
       Thermostat_setPoint = data.result[0].SetPoint;
       if( VERBOSE ) console.log("Thermostat setPoint= " + Thermostat_setPoint);
    }); // DomoticzJsonTalk( JSON_API, func   
    
    HeatingMeter.NbSubMeterToRead = HeatingMeter.IDX_subMeters.length;
    HeatingMeter.MeterUsage1      = HeatingMeter.MeterUsage2      = 0;
    HeatingMeter.MeterUsagePower  = HeatingMeter.MeterReturnPower = 0; 
    
    HeatingMeter.IDX_subMeters.forEach(function(value) {
        JSON_API.path = '/json.htm?type=devices&rid=' + value;
        DomoticzJsonTalk( JSON_API, function(error, data) {  
            if ( error ) return;
            var subMeter = data.result[0].Data.split(";"); 	
            var subMeterPower = parseInt( subMeter[4] ); var subMeterUsage1 = parseInt( subMeter[0] ); var subMeterUsage2 = parseInt( subMeter[1] );	
            if( VERBOSE ) console.log("Heater: " + data.result[0].Name + " - Usage1/Usage2//Power (Wh/Wh//W)=" + subMeter[0] + "/" + subMeter[1] + "//" + subMeter[4] );
            HeatingMeter.MeterUsagePower += subMeterPower; HeatingMeter.MeterUsage1 += subMeterUsage1; HeatingMeter.MeterUsage2 += subMeterUsage2;
            if( --HeatingMeter.NbSubMeterToRead === 0 ) {  // All submeters has been read, we can now update the main heating meter
                if( !ExternalSensor.SensorUnavailable && Thermostat_setPoint !== null ) HeatingMeter.MeterReturnPower = House_H_Ratio * ( Thermostat_setPoint-HouseInternalHeatGain-ExternalSensor.lastTemp );
                if( HeatingMeter.MeterReturnPower < 0 ) HeatingMeter.MeterReturnPower = 0; // No need to heat 
                if( ElecTariff === "HP") HeatingMeter.MeterReturn1 += HeatingMeter.MeterReturnPower/(60/PollingTimer); // Additional Wh forecast to add every n minutes is Power/(60/PollingTimer)
                else HeatingMeter.MeterReturn2 += HeatingMeter.MeterReturnPower/(60/PollingTimer);
                HeatingMeter.MeterReturn1 = parseInt( HeatingMeter.MeterReturn1 ); HeatingMeter.MeterReturn2 = parseInt( HeatingMeter.MeterReturn2 ); HeatingMeter.MeterReturnPower = parseInt( HeatingMeter.MeterReturnPower ); 
                if( VERBOSE ) console.log("\n** UPDATE: Total heating meter - HP/HC Usage/Forecast//Power (Wh/Wh//W)=" + HeatingMeter.MeterUsage1 + "/" + HeatingMeter.MeterUsage2 + "/" + 
                                          HeatingMeter.MeterReturn1  + "/"  + HeatingMeter.MeterReturn2 + "//" + HeatingMeter.MeterUsagePower + "/" + HeatingMeter.MeterReturnPower);
                if( HeatingMeter.MeterUsage1 < HeatingMeter.PreviousMeterUsage1 || HeatingMeter.MeterUsage2 < HeatingMeter.PreviousMeterUsage2) {  
                    console.log("\n[" + new Date() + " HEATING METER INDEX INCONSISTENCY] Latest energy usage computed from ACS712 Heater sensors smaller than DomoticZ one - UPDATE NOT DONE")              
                } else {               
                    JSON_API.path = '/json.htm?type=command&param=udevice&idx=' +  HeatingMeter.IDX_Meter + '&nvalue=0&svalue=' + HeatingMeter.MeterUsage1 + ';' + HeatingMeter.MeterUsage2 + ';' + 
                                    HeatingMeter.MeterReturn1 + ';' + HeatingMeter.MeterReturn2 + ';' + HeatingMeter.MeterUsagePower + ';' + HeatingMeter.MeterReturnPower;
                    DomoticzJsonTalk( JSON_API , function( error, data ) {
                        if( error ) return;
                        HeatingMeter.PreviousMeterUsage1 = HeatingMeter.MeterUsage1; HeatingMeter.PreviousMeterUsage2 = HeatingMeter.MeterUsage2
                        HeatingMeter.MeterPreviousPower  = HeatingMeter.MeterUsagePower;
                        ThermaLoss.Compute();
                        CoolingRate.Compute();
                    });  // DomoticzJsonTalk( JSON_API, func                                
                } //if( HeatingMeter.MeterUsage1 < HeatingMeter.PreviousMeterUsage1 || 
            }  // if( NbSubmeterToRead === 0 ) {
        }); // DomoticzJsonTalk( JSON_API, function(e
    }); // HeatingMeter.IDX_heaterMeters.forEach(function(value) {
  } // UpdateMeter: function() {      
};  // var HeatingMeter = {


var ThermaLoss = {
Device            : "THERMAL",
ID                : 'TLOSS',
TL_Ratio          : 0,      // Energy Wh consumption per °C due to House Thermal loss  (i.e. Wh consumed EACH day for ONE degree difference between indoor and outdoor temperature). TL_Ratio = 24 * 1 (hour) * House_H_Ratio
idxTLRatio        : idxThermalLoss,     
Compute: function() {
    ThermaLoss.TL_Ratio=0;  
    if( HeatingSelector == 0 ||  HeatingSelector == 10 ) return;
    if( !InternalSensor.SensorUnavailable && !ExternalSensor.SensorUnavailable ) {
        var DegresPollingPeriod = InternalSensor.lastTemp-HouseInternalHeatGain-ExternalSensor.lastTemp;
        if( VERBOSE ) console.log("DegresPollingPeriod= " + DegresPollingPeriod);
        if( DegresPollingPeriod > 0 ) ThermaLoss.TL_Ratio = (24* HeatingMeter.MeterPreviousPower / DegresPollingPeriod).toFixed(0);
    } // if( !InternalSensor.SensorUnavailable && !Ext
    if( VERBOSE ) console.log("Thermal Loss ratio=", ThermaLoss.TL_Ratio);
    JSON_API.path = '/json.htm?type=command&param=udevice&idx=' + ThermaLoss.idxTLRatio + '&nvalue=0&svalue=' + ThermaLoss.TL_Ratio;
    DomoticzJsonTalk( JSON_API );
  }  // Compute: function() {
};  // var ThermaLoss = {


var CoolingRate = {
Device            : "THERMAL",
ID                : 'COOLING',
r_Ratio           : [0],    // House Cooling rate
MA_r_Ratio        : 0,      // Moving average r_Ratio computed
r_Ratio_index     : 0,      // Array index of individual subset values
idx_r_Ratio       : idxCoolingRate,    
Compute: function() {
    if( InternalSensor.SensorUnavailable || ExternalSensor.SensorUnavailable ) return;
    if( Math.abs( InternalSensor.previousTemp-ExternalSensor.previousTemp ) <= OUTDOOR_TEMP_REACHED ) return;
    var Ir_Ratio = ( InternalSensor.lastTemp - InternalSensor.previousTemp ) / ( ( InternalSensor.previousTemp-ExternalSensor.previousTemp ) * ( PollingTimer / (24*60) ) ); 
    if( (InternalSensor.previousTemp-ExternalSensor.previousTemp) < 0 ) Ir_Ratio *= -1;
    if( VERBOSE ) console.log("Instantaneous Cooling Rate=", Ir_Ratio);
    if( VERBOSE ) console.log("r_Ratio_index=", CoolingRate.r_Ratio_index);
    if( VERBOSE ) console.log("r_Ratio_subset size=", r_Ratio_subset);
    if( CoolingRate.r_Ratio_index < r_Ratio_subset) { 
        CoolingRate.r_Ratio[CoolingRate.r_Ratio_index++] = Ir_Ratio;
        var i=0;
        for( CoolingRate.MA_r_Ratio=0 ; i < CoolingRate.r_Ratio_index; i++ ) CoolingRate.MA_r_Ratio += CoolingRate.r_Ratio[i];
        CoolingRate.MA_r_Ratio /= CoolingRate.r_Ratio_index; 
    } else {
        CoolingRate.MA_r_Ratio = 0;
        for( CoolingRate.r_Ratio_index=0; CoolingRate.r_Ratio_index < r_Ratio_subset-1; CoolingRate.r_Ratio_index++ ) { 
             CoolingRate.r_Ratio[CoolingRate.r_Ratio_index] = CoolingRate.r_Ratio[CoolingRate.r_Ratio_index+1];  CoolingRate.MA_r_Ratio += CoolingRate.r_Ratio[CoolingRate.r_Ratio_index] 
        } // for( CoolingRate.r_Ratio_index=0;
        CoolingRate.r_Ratio[CoolingRate.r_Ratio_index++] = Ir_Ratio; CoolingRate.MA_r_Ratio += Ir_Ratio;
        CoolingRate.MA_r_Ratio /= r_Ratio_subset; 
    }  // if( r_Ratio_index < r_Ratio_subset
    if( VERBOSE ) console.log("Moving average Cooling Rate=", CoolingRate.MA_r_Ratio); 
    CoolingRate.MA_r_Ratio = CoolingRate.MA_r_Ratio.toFixed(1); 
    JSON_API.path = '/json.htm?type=command&param=udevice&idx=' + CoolingRate.idx_r_Ratio + '&nvalue=0&svalue=' + CoolingRate.MA_r_Ratio;
    DomoticzJsonTalk( JSON_API );        
  } // Compute: function() {      
};  // var ThermaLoss = {

// ** Temp sensors list. 
var MyTempSensors   = [ExternalSensor, InternalSensor];  
var lastDaySeen     = new Date();
var lastDateSeen    = lastDaySeen.toString().slice(0,15);
var lastDaySeen     = lastDaySeen.getDate();

// ***** INIT
console.log("*** " + new Date() + " - Domoticz iot_ESP8266 V0.60 starting ***\n");

// Get latest Heating Forecast stored in DomoticZ
var DzConnect = setInterval(function() { 
    JSON_API.path = '/json.htm?type=devices&rid=' + HeatingMeter.IDX_Meter;
    DomoticzJsonTalk( JSON_API, function( error, data ) {
        if ( error ) return;
        clearInterval( DzConnect );
        HeatingMeter.SensorUnavailable = 0;
        var Meter = data.result[0].Data.split(";"); 	
        HeatingMeter.PreviousMeterUsage1 = HeatingMeter.MeterUsage1 = parseInt( Meter[0] ); HeatingMeter.PreviousMeterUsage2 = HeatingMeter.MeterUsage2 = parseInt( Meter[1] ); 
        HeatingMeter.MeterReturn1 = parseInt( Meter[2] ); HeatingMeter.MeterReturn2 = parseInt( Meter[3] );
        if( VERBOSE ) console.log("Total heating meter data found in DomoticZ - HP/HC Usage/Forecast (Wh): " + HeatingMeter.MeterUsage1 + "/" + HeatingMeter.MeterUsage2 + 
                                  "/" + HeatingMeter.MeterReturn1 + "/" + HeatingMeter.MeterReturn2);
    }); // DomoticzJsonTalk( JSON_API, function(e
},  5000); // setInterval(function() {

// ***** LOOP à la Arduino mode...
setInterval(function(){ // get Temp/Humidity from sensors every n seconds/minutes
   
    // *** Print each day, the total number of DHT22 reading errors
    var Today = new Date().getDate();
    if( Today !== lastDaySeen ) {
       console.log("\n*** " + lastDateSeen + " - Indoor/Outdoor DHT22 NAN Reading Error= " + InternalSensor.NbReadError + "/" + ExternalSensor.NbReadError + " - DJU= " + DayDJU.toFixed(3) + " ***\n" );
       InternalSensor.NbReadError =  ExternalSensor.NbReadError = 0;
       DayDJU = 0;
       lastDaySeen=Today;
       lastDateSeen=(new Date()).toString().slice(0,15);
    }
       
    // *** READ TEMP SENSORS LOOP  *** 
    MyTempSensors.forEach(function(value) {
               var ERROR_MESSAGE = "";                                
               ESP8266DHT22.get(value, function(resp){
                  resp.on('data', function(JSONSensor) {
                     // console.log("Sensor=" + JSONSensor);
                     const CurrentSensorValue = JSON.parse(JSONSensor);                           
                     if( CurrentSensorValue.Temperature_Celsius === "DHT22 ERROR" ) {     // ESP8266 available but a DHT22 reading error occured 
                         value.SensorUnavailable++;  value.NbReadError++;
                         ERROR_MESSAGE="Error - " + value.ID + " has returned a wrong value (NAN). Error code was " + CurrentSensorValue.Humidity_Percentage;   
                         ERROR_MESSAGE=ERROR_MESSAGE.replace(/ /g,"%20");
                         JSON_API.path = '/json.htm?type=command&param=addlogmessage&message=' + ERROR_MESSAGE;
                         DomoticzJsonTalk( JSON_API );  
                     } else { 
                         if( value.previousTemp === null ) value.previousTemp = CurrentSensorValue.Temperature_Celsius; else value.previousTemp=value.lastTemp;
                         value.lastTemp=parseFloat(CurrentSensorValue.Temperature_Celsius);
                         value.lastHumidity=parseFloat(CurrentSensorValue.Humidity_Percentage);
                         value.lastHeatIndex=parseFloat(CurrentSensorValue.HeatIndex_Celsius);
                         value.lastHumStat='0';
                         if (value.lastHumidity <= 30 ) value.lastHumStat='2'
                            else if (value.lastHumidity >= 70 ) value.lastHumStat='3'
                                 else if (value.lastHumidity >= 45 && value.lastHumidity <= 65 ) value.lastHumStat='1';
                         value.SensorUnavailable = 0;
                         if( VERBOSE ) console.log(new Date() + " - Device: " + value.ID + " Temp/PreviousTemp/Hum/HeatIndex= " + value.lastTemp + "/" + value.previousTemp + "/" + value.lastHumidity + "/" + value.lastHeatIndex);                  
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
     
    //  *** DOMOTICZ UPDATE LOOP - Started with a 5s delay regarding temp to let temp sensors reading phase be completed before ***
    setTimeout(function( ){
        DegreesDaysSensor.Compute();
        HeatingMeter.UpdateMeter();    
        MyTempSensors.forEach(function(value) {      
           // Log temperature and humidity     
           if( value.SensorUnavailable )  return;                          
           JSON_API.path = '/json.htm?type=command&param=udevice&idx='+ value.idxTempHum +'&nvalue=0&svalue=' + value.lastTemp + ';' + value.lastHumidity + ';' + value.lastHumStat;
           JSON_API.path = JSON_API.path.replace(/ /g,"");
           DomoticzJsonTalk( JSON_API );  
           // Log HeatIndex temperature  
           JSON_API.path = '/json.htm?type=command&param=udevice&idx='+ value.idxHeatIndex +'&nvalue=0&svalue=' + value.lastHeatIndex;
           JSON_API.path = JSON_API.path.replace(/ /g,"");
           DomoticzJsonTalk( JSON_API );
        }); // MyTempSensors.forEach(f
    }, 5000 );    
    
    // *** TEMP ENSORS HEALTH CHECK LOOP. If we have a temp sensor failure for n hours, raise TEMP SENSOR FAILURE in DomoticZ *** 
    MyTempSensors.forEach(function(value) {      
           if( value.SensorUnavailable >= SensorTimeOut ) {
               JSON_API.path = '/json.htm?type=devices&rid=' + idxTempFailureFlag;
               DomoticzJsonTalk( JSON_API, RaisefailureFlag );               
           }  //  if( value.SensorUnavaila
    }); // TempSensors.forEach(f
    
}, PollingTimer*60000); // setInterval(function(){  get Temp/Humidity from sensors every n mn