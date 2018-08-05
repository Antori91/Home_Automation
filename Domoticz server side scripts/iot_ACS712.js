// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// ***** Domoticz script to compute overall heaters consumption, update heating energy (kWh) forecast and house thermal characteristics (thermal loss and cooling rates) *****
// V0.5  - June 2018
          // IMPROVEMENT : For better accuracy, stop computing instantaneous cooling rate if indoor temp within outdoor temp +/- 0.5°C range
// V0.46 - June 2018
          // IMPROVEMENT : cooling rate moving average subset set to 4 hours
// V0.45 - May 2018 
          // CHANGE : Cooling ratio sign changed when (IndoorTemp-OutdoorTemp) < 0  insteaf of (IndoorTemp-1-OutdoorTemp) < 0
// V0.4 - May 2018 
          // IMPROVEMENT : Reset instantaneous POWER USAGE of a Heater index meter if Domoticz lastseen for this meter is greater than 5 mn. (For both Heaters and HotWaterTank)
// V0.37 - December 2017 
          // CHANGE : if verbose mode set, print heater instantaneous consumption
// V0.36 - November 2017 
          // IMPROVEMENT : Smooth out cooling rate using moving average ( 30 mn subset of instantaneous cooling rate). 
          // CHANGE : Calculate cooling rate even if Degrees.Day < 0 (i.e. Outdoor temp > Indoor temp)
// V0.35 - October 2017 
          // BUG Correction: Avoid to reset MAIN P1 Smart Meter if we failed to reach DomoticZ heater meters (happens if DomoticZ DOWN then UP while this program running)
// V0.3  - September 2017 
          // NEW FEATURE : Compute House Cooling Rate characteristic
// V0.2  - July 2017 
          // IMPROVEMENT : Use an updated H coeff which includes air heating
          // NEW FEATURE : Compute House Thermal Loss characteristic
// V0.1  - July 2017 
          // Initial release 


const VERBOSE                 = false; // logging verbose or not
var   Rollback                = false; // Flag to mention we have to rollback main P1 smart meter to last values and do not update it because we didn't success to reach one of the (or all) DomoticZ heaters meters 
const PollingTimer            = 5;     // Update DomoticZ every n minutes
const SENSOR_TIME_OUT         = 5*60*1000; // 5 mn in ms 

// Energy Formulas used :  
          //   To raise for 1°C the temperature of 1m3 air, thermal energy to supply is 1,256 kJ·= 1,256/3600 kWh (PS: 1 J =  1 Ws)
          //   To raise for 1°C the temperature of 1m3 water, thermal energy to supply is 1,162 KWh // 1.162 * WaterConsommationPerYear * (HotWaterTemp-ColdWaterTemp) ) / ( 365 * 8 );   // = 1,833 KWh during 8 hours per day   

//  House thermal characteristics  
const House_H_Ratio           = 262;  // H ratio used for now (house thermal loss rate)= U*Surface -- Unit W/K or W/°C 
const HouseInternalHeatGain   = 1;    // Auto heating from house residents and domestic appliances -- Unit °C
const HotWaterSetpoint        = 60;   // Setpoint of sanitized water - Unit °C
const IncomingWaterTemp       = 12;   // Temperature of incoming water - Unit °C
const WaterConsumptionPerYear = 96;   // Unit m3

// House/heating items
var Tariff;                        // Electricity supplier tariff to use depending on day period : HP (Heures pleines) or HC (Heures creuses)
var HeatingSelector        = 0;      // Current heating mode selector (0/10/20/30 for OFF/HorsGel/Eco/Confort)
var Steady                 = false;  // Heating in steady mode : set to true if Indoor Temperature has reached one time Thermostat setpoint and HeatingSelector is ECO or Confort
var Thermostat_setPoint    = -150;   // Initialized to -150 to indicate we are at the first run
var OutdoorTemp            = -150;   // Current Outdoor temperature  (latest sensor polling)
var PreviousOutdoorTemp    = -150;   // Previous Outdoor Temperature (previous sensor polling)  
var IndoorTemp             = -150;   // Current House Indoor Temperature
var PreviousIndoorTemp     = -150;   // Previous House Indoor Temperature
var TL_Ratio               = 0;      // Energy Wh consumption per °C due to House Thermal loss  (i.e. Wh consumed EACH day for ONE degree difference between indoor and outdoor temperature). TL_Ratio = 24 * 1 (hour) * House_H_Ratio
const r_Ratio_subset       = (4 * 60/PollingTimer); // 4 hours subset moving average
var r_Ratio                = [0];    // House Cooling rate. 30 mn individual subset values
var MA_r_Ratio             = 0;      // Moving average r_Ratio computed
var r_Ratio_index          = 0;      // to access individual subset values
const OUTDOOR_TEMP_REACHED = 0.5;   

// DomoticZ meter IDX=33 - Overall energy consumption and forecast 
var USAGE_HP             = 0;      // Current value of energy Wh actually used (overall meter)
var USAGE_HC             = 0;
var PREVIOUS_USAGE_HP    = 0;      // Previous known value of energy Wh actually used (overall meter)
var PREVIOUS_USAGE_HC    = 0;
var FORECAST_HP          = 0;      // Current Energy Wh needed forecast
var FORECAST_HC          = 0;
var TOTAL_POWER          = 0;      // Actual Power W in use
var TOTAL_4CAST_POWER    = 0;      // Power W needed forecast

// DomoticZ heater meters
const IDX_Convecteur          = ['27', '28', '29', '30', '31', '35', '36', '37', '38']; 
const IDX_OverallHeatersMeter = 33;
const IDX_HotWaterTank        = 34;        

// JSON URLs to read/write DomoticZ
var httpDomoticz = require('http');
var SetHeaterMeter = {
host: 'localhost',
port: 8084,
path: '',
base_path: '/json.htm?type=command&param=udevice&idx=' // + IDX + '&nvalue=0&svalue=' + USAGE_HP + ';' + USAGE_HC+ ';' + FORECAST_HP + ';' + FORECAST_HC + ';' + TOTAL_POWER + ';' + TOTAL_4CAST_POWER
};
var GetOverallHeatersMeter = {
host: 'localhost',
port: 8084,
path: '/json.htm?type=devices&rid=33'
};
var GetHeaterMeter = {
host: 'localhost',
port: 8084,
path: '',
base_path: '/json.htm?type=devices&rid=' // + IDX
};
var log_H_Ratio = {
host: 'localhost',
port: 8084,
path: '',
base_path: '/json.htm?type=command&param=udevice&idx=44&nvalue=0&svalue=' // +  TL_Ratio,
};
var log_r_Ratio = {
host: 'localhost',
port: 8084,
path: '',
base_path: '/json.htm?type=command&param=udevice&idx=49&nvalue=0&svalue=' // +  r_Ratio,
};
var GetHeatingSelector = {
host: 'localhost',
port: 8084,
path: '/json.htm?type=devices&rid=17'
};
var GetSetPoint = {
host: 'localhost',
port: 8084,
path: '/json.htm?type=devices&rid=16'
};
var GetOutdoorTemp = {
host: 'localhost',
port: 8084,
path: '/json.htm?type=devices&rid=8'
};
var GetIndoorTemp = {
host: 'localhost',
port: 8084,
path: '/json.htm?type=devices&rid=9'
};
var GetElectricityTariff = {
host: 'localhost',
port: 8084,
path: '/json.htm?type=devices&rid=32'
};


// ******* INIT
console.log("*** " + new Date() + " - Domoticz iot_ACS712 started ***\n");
httpDomoticz.get(GetOverallHeatersMeter, function(resp) {   // Get latest Energy forecast stored
     resp.on('data', function(JSONoverall){ 
         const overall = JSON.parse(JSONoverall); 
         var data = overall.result[0].Data.split(";");
         USAGE_HP = parseInt( data[0] );
         USAGE_HC = parseInt( data[1] );          
         FORECAST_HP = parseInt( data[2] );
         FORECAST_HC = parseInt( data[3] );      
         if( VERBOSE ) console.log("Latest Overall heaters meter values - HP/HC Consumption/Forecast (Wh): " + USAGE_HP + "/" + USAGE_HC + "/" + FORECAST_HP + "/" + FORECAST_HC);
     });
}).on("error", function(e){
     console.log("Error - Can't get DomoticZ Main Energy Consumption/Forecast Meter at starting time: " + e.message);
     console.log("\n=== PROGRAM ABORT - Check that DomoticZ is up and running before trying to restart ===");
     process.exit();
});
  
// ******* LOOP à la Arduino mode...
setInterval(function(){ // compute overall Heater consumptions and update h/r ratios every n minutes
  
  if( VERBOSE ) console.log("\n*** " + new Date() + " - DOMOTICZ DEVICES UPDATE ***");
 
  // *** If not first run, log energy consumption/forecast and H/r ratios updates *** 
  if( Thermostat_setPoint != -150 ) {
      if( !Rollback )  {  // We didn't success the last loop time to reach DomoticZ heaters meters, so rollback Main P1 smart meter to last values and do not update this main meter
          TOTAL_4CAST_POWER = House_H_Ratio * ( Thermostat_setPoint-HouseInternalHeatGain-PreviousOutdoorTemp );
          if( TOTAL_4CAST_POWER < 0 ) TOTAL_4CAST_POWER = 0; // No need to heat 
          if( Tariff === "HP") FORECAST_HP += TOTAL_4CAST_POWER/(60/PollingTimer); // Additional Wh Consumption to add every n minutes is Power/(60/PollingTimer)
          else FORECAST_HC += TOTAL_4CAST_POWER/(60/PollingTimer);
          FORECAST_HP = parseInt( FORECAST_HP ); FORECAST_HC = parseInt( FORECAST_HC ); TOTAL_4CAST_POWER = parseInt( TOTAL_4CAST_POWER ); 
          if( VERBOSE ) console.log("Overall heaters - PREVIOUS HP/HC Consumption (Wh)=" + PREVIOUS_USAGE_HP + "/" + PREVIOUS_USAGE_HC);
          if( VERBOSE ) console.log("Overall heaters - CURRENT HP/HC Consumption/Forecast//Power (Wh/Wh//W)=" + USAGE_HP + "/" + USAGE_HC + "/" + FORECAST_HP  + "/"  + FORECAST_HC + "//" + TOTAL_POWER + "/" + TOTAL_4CAST_POWER);  
          SetHeaterMeter.path = SetHeaterMeter.base_path + IDX_OverallHeatersMeter + '&nvalue=0&svalue=' + USAGE_HP + ';' + USAGE_HC+ ';' + FORECAST_HP + ';' + FORECAST_HC + ';' + TOTAL_POWER + ';' + TOTAL_4CAST_POWER;
          httpDomoticz.get( SetHeaterMeter, function(resp){
             resp.on('data', function(ReturnStatus){ 
                if( VERBOSE ) console.log("Overall Heaters Consumption and Forecast logged " + ReturnStatus);
             });
          }).on("error", function(e){
               console.log("Error - Can't update DomoticZ Overall Heaters Consumption and Forecast : " + e.message);
          });
      }  else {  // if( !Rollback )
          USAGE_HP = PREVIOUS_USAGE_HP; USAGE_HC = PREVIOUS_USAGE_HC;
          console.log("=== ROLLBACK DONE. One of the DomoticZ heater meters was unavailable ===");
          console.log("Overall heaters - PREVIOUS HP/HC Consumption (Wh)=" + PREVIOUS_USAGE_HP + "/" + PREVIOUS_USAGE_HC);
          console.log("Overall heaters - CURRENT HP/HC Consumption/Forecast//Power (Wh/Wh//W)=" + USAGE_HP + "/" + USAGE_HC + "/" + FORECAST_HP  + "/"  + FORECAST_HC + "//" + TOTAL_POWER + "/" + TOTAL_4CAST_POWER); 
      } //  if( !Rollback )
      
      // Compute and log H and r ratios (using DegresDays calculated beginning of polling period)
      var DegresDay = ( PreviousIndoorTemp-HouseInternalHeatGain-PreviousOutdoorTemp ) * ( PollingTimer / (24*60) );
      // if( DegresDay < 0 ) DegresDay = 0;
      if( VERBOSE ) console.log("DegresDay=" + DegresDay);
      TL_Ratio=0;
      if( Steady && !Rollback ) { // compute and log H ratio      		 
      		 if( DegresDay > 0) {
      			  TL_Ratio = ( USAGE_HP + USAGE_HC - PREVIOUS_USAGE_HP - PREVIOUS_USAGE_HC ) / DegresDay;
      			  TL_Ratio=TL_Ratio.toFixed(0);
      		 }
      } // if( Steady  { // compute and log H ratio
      if( VERBOSE ) console.log("Thermal Loss ratio=", TL_Ratio);
      log_H_Ratio.path = log_H_Ratio.base_path + TL_Ratio;
      httpDomoticz.get( log_H_Ratio, function(resp){
        resp.on('data', function(ReturnStatus) {
           if( VERBOSE ) console.log("Thermal Loss ratio updated " + ReturnStatus);
        });
      }).on("error", function(e){
           console.log("Error - Can't update DomoticZ Thermal Loss ratio: " + e.message);
      });    
            
      // if( HeatingSelector === 0 ||  HeatingSelector === 10 ) { // compute and log -1 * r_ratio  
             if( Math.abs( PreviousIndoorTemp-PreviousOutdoorTemp ) > OUTDOOR_TEMP_REACHED ) {  
                  var Ir_Ratio = ( IndoorTemp - PreviousIndoorTemp ) / ( ( PreviousIndoorTemp-PreviousOutdoorTemp ) * ( PollingTimer / (24*60) ) ); 
                  if( (PreviousIndoorTemp-PreviousOutdoorTemp) < 0 ) Ir_Ratio *= -1;
                  if( VERBOSE ) console.log("Instantaneous Cooling Rate=", Ir_Ratio);
                  if( VERBOSE ) console.log("r_Ratio_index=", r_Ratio_index);
                  if( VERBOSE ) console.log("r_Ratio_subset size=", r_Ratio_subset);
                  if( r_Ratio_index < r_Ratio_subset) { 
                     r_Ratio[r_Ratio_index++] = Ir_Ratio;
                     for( var i=0, MA_r_Ratio=0 ; i < r_Ratio_index; i++ ) MA_r_Ratio += r_Ratio[i];
                     MA_r_Ratio /= r_Ratio_index; 
                  }
                  else {
                     MA_r_Ratio = 0;
                     for( r_Ratio_index=0; r_Ratio_index < r_Ratio_subset-1; r_Ratio_index++ ) { r_Ratio[r_Ratio_index] = r_Ratio[r_Ratio_index+1];  MA_r_Ratio += r_Ratio[r_Ratio_index] }
                     r_Ratio[r_Ratio_index++] = Ir_Ratio; MA_r_Ratio += Ir_Ratio;
                     MA_r_Ratio /= r_Ratio_subset; 
                  }  // if( r_Ratio_index < r_Ratio_subset
                  if( VERBOSE ) console.log("Moving average Cooling Rate=", MA_r_Ratio); 
                  MA_r_Ratio = MA_r_Ratio.toFixed(1);               
                  log_r_Ratio.path = log_r_Ratio.base_path + MA_r_Ratio;
                  httpDomoticz.get( log_r_Ratio, function(resp){
                      resp.on('data', function(ReturnStatus){ 
                          if( VERBOSE ) console.log("Cooling Rate updated " + ReturnStatus);
                       });
                  }).on("error", function(e){
                        console.log("Error - Can't update DomoticZ Cooling Rate: " + e.message);
                  });
             } // if( Math.abs( PreviousIndoorTemp-PreviousOutdoorTemp ) > OUTDOOR_TEMP_REACHED ) {  
      // } // if( HeatingSelector === 0 ||  HeatingSelector === 10 ) { // compute and log 1/r ratio

  } // *** If not first run, log energy consumption/forecast and H/r ratios update *** 
   
  // *** Get updates of HP/HC tariff, heating mode selector, thermostat setpoint and indoor/outdoor temperatures ***
  httpDomoticz.get(GetElectricityTariff, function(resp){
     resp.on('data', function(JSONHPHC){ 
       const HPHC = JSON.parse(JSONHPHC); 
       Tariff = "HP";
       if( HPHC.result[0].Data === "Off") Tariff= "HC";
       if( VERBOSE ) console.log("Electricity Tariff=" + Tariff);
     });
  }).on("error", function(e){
       console.log("Error - Can't get DomoticZ Electricity (HP/HC) Tariff: " + e.message);
  });
  httpDomoticz.get(GetHeatingSelector, function(resp){
     resp.on('data', function(JSONHeatingLevel){ 
       const HeatingLevel = JSON.parse(JSONHeatingLevel); 
       HeatingSelector = HeatingLevel.result[0].Level;
       if( VERBOSE ) console.log("Heating Selector=" + HeatingSelector);
     });
  }).on("error", function(e){
       console.log("Error - Can't get DomoticZ Heating Mode Selector: " + e.message);
  })  
  httpDomoticz.get(GetSetPoint, function(resp){
     resp.on('data', function(JSONThermostat){ 
       const Thermostat = JSON.parse(JSONThermostat); 
       Thermostat_setPoint = Thermostat.result[0].SetPoint;
       if( VERBOSE ) console.log("Thermostat setPoint=" + Thermostat_setPoint);
     });
  }).on("error", function(e){
       console.log("Error - Can't get DomoticZ Thermostat setPoint: " + e.message);
  });
  httpDomoticz.get(GetOutdoorTemp, function(resp){
     resp.on('data', function(JSONTemp){ 
       const Temp = JSON.parse(JSONTemp); 
       if( PreviousOutdoorTemp === -150 ) PreviousOutdoorTemp = Temp.result[0].Temp; else PreviousOutdoorTemp = OutdoorTemp;
       OutdoorTemp = Temp.result[0].Temp;
       if( VERBOSE ) console.log("Previous Outdoor Temperature=" + PreviousOutdoorTemp);
       if( VERBOSE ) console.log("Current Outdoor Temperature=" + OutdoorTemp);
     });
  }).on("error", function(e){
       console.log("Error - Can't get DomoticZ Outdoor Temperature: " + e.message);
  });
  httpDomoticz.get(GetIndoorTemp, function(resp){
     resp.on('data', function(JSONTemp){ 
       const Temp = JSON.parse(JSONTemp); 
       if( PreviousIndoorTemp === -150 ) PreviousIndoorTemp = Temp.result[0].Temp; else PreviousIndoorTemp = IndoorTemp;
       IndoorTemp = Temp.result[0].Temp;
       if( VERBOSE ) console.log("Previous Indoor Temperature=" + PreviousIndoorTemp);
       if( VERBOSE ) console.log("Current Indoor Temperature=" + IndoorTemp);
     });
  }).on("error", function(e){
       console.log("Error - Can't get DomoticZ Indoor Temperature: " + e.message);
  });
  
  // Are we at steady phase, if yes set it 
  if( ( HeatingSelector === 20 ||  HeatingSelector === 30 ) && IndoorTemp >= Thermostat_setPoint ) Steady = true;
  if( HeatingSelector === 0 ||  HeatingSelector === 10 ) Steady = false;
  if( VERBOSE ) console.log("Heating Steady Phase=" + Steady);
  
  // Get update of each heater meter and compute overall consumption
  // Also check lastSeen of each Heaters and eventually reset their Power Usage
  var cTime = ( new Date() ).getTime(); // Return number of milliseconds since January 1,1970,00:00 UTC
  Rollback=false;
  PREVIOUS_USAGE_HP = USAGE_HP; PREVIOUS_USAGE_HC = USAGE_HC;
  TOTAL_POWER = 0; USAGE_HP = 0; USAGE_HC= 0;
  for(var i = 0 ; i < IDX_Convecteur.length ; i++) {
    GetHeaterMeter.path = GetHeaterMeter.base_path + IDX_Convecteur[i];
    httpDomoticz.get(GetHeaterMeter, function(resp){
      resp.on('data', function(JSONHeaterMeter){
         const HeaterMeter = JSON.parse(JSONHeaterMeter);   
         var data = HeaterMeter.result[0].Data.split(";");  
         var PowerUsage = parseInt( data[4] );
         var HPindex    = parseInt( data[0] );
         var HCindex    = parseInt( data[1] );
         if( VERBOSE ) console.log("Heater: " + HeaterMeter.result[0].Name + " - HP/HC Consumption//Power (Wh/Wh//W)=" + data[0] + "/" + data[1] + "//" + data[4] );  
         TOTAL_POWER += parseInt( data[4] );
         USAGE_HP    += parseInt( data[0] );
         USAGE_HC    += parseInt( data[1] );
         var lastSeen = ( new Date( HeaterMeter.result[0].LastUpdate ) ).getTime();
         if( cTime-lastSeen > SENSOR_TIME_OUT && PowerUsage != 0 ) {
             if( VERBOSE ) console.log("Resetting DomoticZ Meter Power Usage for Heater: " + HeaterMeter.result[0].Name);
             SetHeaterMeter.path = SetHeaterMeter.base_path + HeaterMeter.result[0].idx + '&nvalue=0&svalue=' + HPindex + ';' + HCindex + ';0;0;0;0';
             httpDomoticz.get( SetHeaterMeter, function(resp){
                resp.on('data', function(ReturnStatus){ 
                if( VERBOSE ) console.log("DomoticZ Meter Power Usage Reset for a Heater: " + ReturnStatus);
             });
             }).on("error", function(e){
                console.log("Error - Can't reset one the DomoticZ Heater Meter Power Usage: " + e.message);
             });
         } // if( cTime-lastSeen > SENSOR_TIME_OUT ) {
      }); //resp.on('data', function(JSONSHeater
    }).on("error", function(e){
         console.log("Error - Can't get one of the DomoticZ Heater Meter: " + e.message);
         Rollback=true;
    }); // httpDomoticz.get(GetHeaterMeter, function(resp){
  } // for(var i = 0 ; i < IDX_Convecteur
  
  // Check lastSeen of HotWatertank and eventually reset its Power Usage
  GetHeaterMeter.path = GetHeaterMeter.base_path + IDX_HotWaterTank;
  httpDomoticz.get(GetHeaterMeter, function(resp){
    resp.on('data', function(JSONHeaterMeter){
         const HeaterMeter = JSON.parse(JSONHeaterMeter);
         var data = HeaterMeter.result[0].Data.split(";"); 	
         var PowerUsage = parseInt( data[4] );
         var HPindex    = parseInt( data[0] );
         var HCindex    = parseInt( data[1] );	
         if( VERBOSE ) console.log("Heater: " + HeaterMeter.result[0].Name + " - HP/HC Consumption//Power (Wh/Wh//W)=" + data[0] + "/" + data[1] + "//" + data[4] );
         var lastSeen = ( new Date( HeaterMeter.result[0].LastUpdate ) ).getTime();
         if( cTime-lastSeen > SENSOR_TIME_OUT && PowerUsage != 0 ) {
             SetHeaterMeter.path = SetHeaterMeter.base_path + IDX_HotWaterTank + '&nvalue=0&svalue=' + HPindex + ';' + HCindex + ';0;0;0;0';
             httpDomoticz.get( SetHeaterMeter, function(resp){
                resp.on('data', function(ReturnStatus){ 
                if( VERBOSE ) console.log("DomoticZ Index Meter Power Usage Reset for HotWaterTank: " + ReturnStatus);
             });
             }).on("error", function(e){
                console.log("Error - Can't reset DomoticZ Index Meter Power Usage for HotWaterTank: " + e.message);
             });
         } // if( cTime-lastSeen > SENSOR_TIME_OUT ) {
    }); //resp.on('data', function(JSONSHeater
  }).on("error", function(e){
       console.log("Error - Can't get DomoticZ Heater Meter for HotWaterTank - " + e.message);
  }); // httpDomoticz.get(GetHeaterMeter, function(resp){

}, PollingTimer*60000); // setInterval(function(){  

