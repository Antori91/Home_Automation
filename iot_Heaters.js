// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// ***** Domoticz script to manage and monitor ESP8266-ADC+ACS712/heaters *****
// V0.8 - March 2018

const VERBOSE = false; // logging verbose or not

var httpDomoticz = require('http');
var JSON_API     = {
host: 'localhost',         //[$$DOMOTICZ_PARAMETER]
port: 8084,                //[$$DOMOTICZ_PARAMETER]
path: '/'
};

const passwordProtectedDevice   = 'XXXX';           //[$$DOMOTICZ_PARAMETER]
const idxHeaterFailureFlag      = '46';             //[$$DOMOTICZ_PARAMETER]
const idxUnactiveHeatersDisplay = '47';             //[$$DOMOTICZ_PARAMETER]
const idxActivateHeaters        = '48';             //[$$DOMOTICZ_PARAMETER]
const idxUnactivateHeaters      = '23';             //[$$DOMOTICZ_PARAMETER]
const Var_UnactiveHeaters       = '1';              //[$$DOMOTICZ_PARAMETER]
const Varname_UnactiveHeaters   = 'HeatersActive';  //[$$DOMOTICZ_PARAMETER]

var DomoticzJsonTalk = function( JsonUrl, callBack, objectToCompute ) {    // Function to talk to DomoticZ. Do Callback if any 
   var savedURL=JSON.stringify(JsonUrl);
   if( VERBOSE ) console.log("\n** DomoticZ URL request=" + savedURL );
   httpDomoticz.get(JsonUrl, function(resp){
      resp.on('data', function(ReturnData){ 
         if( VERBOSE ) console.log("\nDomoticZ answer=" + ReturnData);
         if( callBack ) callBack( null, JSON.parse(ReturnData), objectToCompute );
      });
   }).on("error", function(e){
         console.log("\n** Error - " + e.message + "\nCan't reach DomoticZ with URL:" + savedURL );     
         if( callBack ) callBack( e, null, objectToCompute );
   });
};   // function DomoticzJsonTalk( JsonUrl ) 

var RaisefailureFlag = function( error, data ) {
  if( error ) return;
  if( data.result[0].Status === "Off" ) {
       JSON_API.path = '/json.htm?type=command&param=switchlight&idx=' + idxHeaterFailureFlag + '&switchcmd=On&passcode=' + passwordProtectedDevice;
       JSON_API.path = JSON_API.path.replace(/ /g,"");
       DomoticzJsonTalk( JSON_API );                
  }  // if( GetAlarmIDX.result[0].Status === 
}; // var RaisefailureFlag = fu

var GetHeatersStatus = function( error, data, Hstatus ) {
  if( error ) return;
  HeatingStatus = JSON.parse( Hstatus=data.result[0].Value ); 
  if( VERBOSE ) console.log("Heaters Status stored in DomoticZ is ", HeatingStatus ); 
  if( HeatingStatus != null && HeatingStatus != "" && HeatingStatus != "UNKNOWN" )
    myHeaters.forEach( function( value ) { 
       if( VERBOSE ) console.log("Heater " + value.HeaterName + " - Latest Status was "  + HeatingStatus[(""+value.IDX)] ); 
       if(value.IDX != -1) value.Active = HeatingStatus[(""+value.IDX)];
    });
  myHeaters.forEach( function( value ) { 
        if(value.IDX != -1) HeatingStatusTxt = HeatingStatusTxt + value.HeaterName + ":" + value.Active + " "; 
  });  
  HeatingStatus = ""+Hstatus; // CAST HeatingStatus to string (required for MQTT publish)
  JSON_API.path = '/json.htm?type=command&param=udevice&idx=' + idxUnactiveHeatersDisplay + '&nvalue=0&svalue=' + HeatingStatusTxt;
  JSON_API.path = JSON_API.path.replace(/ /g,"%20");
  DomoticzJsonTalk( JSON_API );
}; // var GetHeatingStatus = function( error, dat

function heater( MacAddress, IDX, Nominal, HeaterName, Zone1, Zone2 ) {
    this.MacAddress=MacAddress; this.HeaterName=HeaterName; this.IDX = IDX; this.Nominal = Nominal; this.Active = "On"; this.DeviceFault = 0; 
    this.Zone1 = Zone1; this.Zone2 = Zone2;    // Each heater may belong to two zones : -1 no Zone belonging, 20 Entrée-Cuisine-Sam, 30	Salon, 40	RDC, 50	CH4, 60	CH3, 70	CH2, 80	Sdb, 90	Parental, 100	1ER
    this.NumberOf_OFFRead = 0; this.OFF_VadcMin = 1024; this.OFF_VadcMax = 0; this.OFF_PowerAverage = 0; this.OFF_PowerMin = Nominal;     this.OFF_PowerMax = 0;
    this.NumberOf_ONRead = 0;  this.ON_VadcMin  = 1024; this.ON_VadcMax  = 0; this.ON_PowerAverage  = 0; this.ON_PowerMin  = 2 * Nominal; this.ON_PowerMax  = 0;
    this.RaiseFaultFlag = function() { this.DeviceFault = 1; }
    this.log = function(Vadc_Min, Vadc_Max) {
           this.DeviceFault = 0;
           var HeaterPower = parseInt( 230 * ( ( 4.3 * 0.707 * ( (Vadc_Max - Vadc_Min) / 2 ) / 1023  ) / 0.100 ) );
           if( VERBOSE ) console.log("Heater Power ADC-ACS712 reading = " + HeaterPower + " Watts");
           if( HeaterPower >= this.Nominal/2 )  {
                this.ON_PowerAverage = (this.ON_PowerAverage * this.NumberOf_ONRead++ + HeaterPower)/this.NumberOf_ONRead;
                if( Vadc_Max > this.ON_VadcMax )      this.ON_VadcMax  = Vadc_Max;
                if( Vadc_Min < this.ON_VadcMin )      this.ON_VadcMin  = Vadc_Min;
                if( HeaterPower > this.ON_PowerMax )  this.ON_PowerMax = HeaterPower;
                if( HeaterPower < this.ON_PowerMin )  this.ON_PowerMin = HeaterPower;
           } else {
                this.OFF_PowerAverage = (this.OFF_PowerAverage * this.NumberOf_OFFRead++ + HeaterPower)/this.NumberOf_OFFRead;
                if( Vadc_Max > this.OFF_VadcMax )     this.OFF_VadcMax  = Vadc_Max;
                if( Vadc_Min < this.OFF_VadcMin )     this.OFF_VadcMin  = Vadc_Min;
                if( HeaterPower > this.OFF_PowerMax ) this.OFF_PowerMax = HeaterPower;
                if( HeaterPower < this.OFF_PowerMin ) this.OFF_PowerMin = HeaterPower;
           }
    } // heater.log method
}   // function heater( IDX

// ***** INIT ***************
console.log("*** " + new Date() + " - Domoticz iot_Heaters started ***\n");
const PollingTimer     = 60; // Print heater stats every n minutes
const HEATER_OFFLINE   = "Heater went Offline";
const LIGHTING_ONLINE  = "Lighting Online";
const LIGHTING_OFFLINE = "Lighting went Offline";
var   HeatingStatus    = ""; // Heaters/Zones active (Command sent via MQTT to heaters)
var   HeatingStatusTxt = ""; // Heaters/Zones active (Text format to display to DomoticZ)
var   myHeaters        = [ new heater("3A73F0", 28, 1500, "ENTREE", "10", "40" ),     new heater("3B2071", 29, 1500, "CUISINE", "10", "40"),     new heater("3B1D5F", 27, 1500, "SALLE A MANGER", "20", "40"),
                           new heater("FA9ECE", 30, 1500, "SALON SUD", "30", "40"),   new heater("3B1A D", 31, 1500, "SALON NORD", "30", "40"),
                           new heater("94D6A3", 35, 1500, "CH4", "50", "100"),      new heater("94CD66", 36, 1000, "CH3", "60", "100" ),   new heater("94CDC2", 37, 1500, "CH2", "70", "100"), 
                           new heater("9497B1", -1, 500,  "SDB", "80", "100"),        new heater("65DEF6", 38, 1500, "PARENTAL", "90", "100"),   new heater("412A10", 34, 2000, "ECS", "-1", "-1") ];
var   myLighting       = 0;  // Lighting status 0=OK, 1=Failed less than 1 hour ago, 2=Failed more than 1 hour ago, i.e alarm to raise 

// Get active Heaters/Zones and display it 
JSON_API.path = '/json.htm?type=command&param=getuservariable&idx=' + Var_UnactiveHeaters;
JSON_API.path = JSON_API.path.replace(/ /g,"%20");
DomoticzJsonTalk( JSON_API, GetHeatersStatus, HeatingStatus );
// Set the two Heater Zones Activation selector switchs to [Ack]
JSON_API.path = '/json.htm?type=command&param=switchlight&idx=' + idxActivateHeaters   + '&switchcmd=Set%20Level&level=110';
DomoticzJsonTalk( JSON_API );
JSON_API.path = '/json.htm?type=command&param=switchlight&idx=' + idxUnactivateHeaters + '&switchcmd=Set%20Level&level=110';
DomoticzJsonTalk( JSON_API );

// Start MQTT
var mqtt = require('mqtt');
var client  = mqtt.connect('mqtt://localhost');    //  //[$$MQTT_PARAMETER]

client.on('connect', function () {
  client.subscribe(['domoticz/in', 'domoticz/out', 'heating/in']);
})  // client.on('co
// ***** END INIT ***************

// *****  MQTT Callback ***************
client.on('message', function (topic, message) {
     var hzoneModified = false;  // At least One heating zone was newly activated or unactivated at DomoticZ side
     var JSONmessage=JSON.parse(message);
     message=message.toString() // message is Buffer 
     if( VERBOSE ) console.log("\n*** New MQTT message received: " + message);
    
     // *** Message coming from DomoticZ about Heaters/Zones to activate/unactivate
     if( message.indexOf('"name" : "Horaires/Stop Chauffage"') != -1 ) { 
          hzoneModified = true;
          var UnactivatedZone =  JSONmessage.svalue1;
          if( UnactivatedZone === "0" ) { 
              if( VERBOSE ) console.log("Activate all Heaters - Reset Command from DomoticZ"); 
              myHeaters.forEach( function( value ) { value.Active="On" } );
          } else {     
              if( VERBOSE ) console.log("Heaters Unactivation Command received from DomoticZ for Heater(s) belonging to Zone: " + UnactivatedZone);   
              myHeaters.forEach( function( value ) { if(value.Zone1 === UnactivatedZone || value.Zone2 === UnactivatedZone )  value.Active="Off" } );    
          }            
     } // if( message.indexOf('"name" : "Horaires/Stop Chauffage"') != -1 ) { 
     if( message.indexOf('"name" : "Horaires/Start Chauffage"') != -1 ) { 
          hzoneModified = true;
          var ActivatedZone =  JSONmessage.svalue1;
          if( ActivatedZone === "0" ) { 
              if( VERBOSE ) console.log("Activate all Heaters - Reset Command from DomoticZ"); 
              myHeaters.forEach( function( value ) { value.Active="On" } );
          } else {     
              if( VERBOSE ) console.log("Heaters Activation Command received from DomoticZ for Heater(s) belonging to Zone: " + ActivatedZone);   
              myHeaters.forEach( function( value ) { if(value.Zone1 === ActivatedZone || value.Zone2 === ActivatedZone )  value.Active="On" } );    
          }            
     } // if( message.indexOf('"name" : "Horaires/Start Chauffage"') != -1 )  
     
     if( hzoneModified ) { 
          HeatingStatus='{"command" : "activateheaters"';   HeatingStatusTxt="";
          myHeaters.forEach( function( value ) { 
                    if(value.IDX != -1) { 
                         HeatingStatus    = HeatingStatus + ', "' + value.IDX + '" : "' + value.Active + '"'; 
                         HeatingStatusTxt = HeatingStatusTxt + value.HeaterName + ":" + value.Active + " "; 
                    }
          });
          HeatingStatus += "}";
          client.publish('heating/out', HeatingStatus);
          if( VERBOSE ) console.log("Published: "  + HeatingStatus);
          // Save the heaters context in a Domoticz user variable
          JSON_API.path = '/json.htm?type=command&param=updateuservariable&vname=' + Varname_UnactiveHeaters + '&vtype=2&vvalue=' + HeatingStatus;
          JSON_API.path = JSON_API.path.replace(/ /g,"%20");
          DomoticzJsonTalk( JSON_API );
          // Update the display GUI
          JSON_API.path = '/json.htm?type=command&param=udevice&idx=' + idxUnactiveHeatersDisplay + '&nvalue=0&svalue=' + HeatingStatusTxt;
          JSON_API.path = JSON_API.path.replace(/ /g,"%20");
          DomoticzJsonTalk( JSON_API );
     }  // if( hzoneModified ) { 
     
     // *** Message coming from a heater/ACS712 or Lighting about its power consumption (for Heater) OR its testament (!)
     if( message.indexOf("addlogmessage") != -1 ) { 
     
        if( VERBOSE ) console.log("DomoticZ Log Message type: testing now if sender is LIGHTING...");
        var pos = message.indexOf(LIGHTING_OFFLINE);
        if( pos != -1 )  { // Raise alarm, MQTT said LIGHTING is dead !
            console.log("\n*** " + new Date() + " == LIGHTING FAILURE ==");
            console.log( message + "\n");
            myLighting = 1;
        } 
        pos = message.indexOf(LIGHTING_ONLINE);
        if( pos != -1 )  { // Reset alarm, LIGHTING is back !
            console.log("\n*** " + new Date() + " == LIGHTING ONLINE ==");
            console.log( message + "\n");
            myLighting = 0;
        } 
        
        if( VERBOSE ) console.log("DomoticZ Log Message type: testing now if sender is a Heater/ACS712...");
        var pos = message.indexOf(HEATER_OFFLINE);
        if( pos != -1 )  { // Raise alarm, MQTT said this one is dead !
            console.log("\n*** " + new Date() + " == HEATER FAILURE ==");
            console.log( message + "\n");
            pos = message.indexOf("@192");
            var mac6 = message.slice(pos-6,pos);
            if( VERBOSE ) console.log( "Heater found Offline - mac6=" + mac6 + "\n");
            myHeaters.forEach(function( value ) { 
                  if( value.MacAddress === mac6 ) {
                      if( VERBOSE ) console.log("Internal Device Fault raised for Heater IDX/MacAddress=" +  value.IDX + "/" + value.MacAddress );
                      value.RaiseFaultFlag();
                  }    
            });                                  
        } else { // Non heater failure message
            var IDXsmsg;
            myHeaters.forEach(function( value ) {
                  IDXsmsg="idx : "+ value.IDX;
                  pos = message.indexOf(IDXsmsg);
                  if( VERBOSE ) console.log("IDXsmsg/pos=" + IDXsmsg + "/" + pos);
                  if( pos != -1 ) {
                       pos = message.indexOf("Vadc_Min/Max=")
                       var posSlash = message.indexOf("/",pos+12)
                       var posQMark = message.indexOf('"',posSlash)
                       var Vadc_Min = message.slice(pos+13,posSlash);
                       var Vadc_Max = message.slice(posSlash+1,posQMark);
                       if( VERBOSE ) console.log("Is actually an ACS712 log: IDX/Vadc_Min/Max=" +  value.IDX + "/" + Vadc_Min + "/" + Vadc_Max );
                       value.log(Vadc_Min, Vadc_Max);                   
                  }
            });
        }  // else if( pos != -1 )  { // Raise alarm, MQTT said this one is dead !    
     } //  if( message.indexOf("addlogmessage") != -1 ) { // Message coming from heater/ACS712 about its power consumption
     
}) // client.on('message', functio

// ***** LOOP à la Arduino mode...  ***************
setInterval(function(){ // print consumption stats and unactivate status every n minutes
    console.log("\n*** " + new Date() + " - Heaters Consumption Update:");
    myHeaters.forEach( function( value ) { console.log(value) } );
    
    console.log("\n*** " + new Date() + " - Active Heaters/Zones Status:");
    console.log(HeatingStatus);
    client.publish('heating/out', HeatingStatus);
    JSON_API.path = '/json.htm?type=command&param=udevice&idx=' + idxUnactiveHeatersDisplay + '&nvalue=0&svalue=' + HeatingStatusTxt;
    JSON_API.path = JSON_API.path.replace(/ /g,"%20");
    DomoticzJsonTalk( JSON_API );
    
    // Set the two Heater Zones Activation selector switchs to [Ack]
    JSON_API.path = '/json.htm?type=command&param=switchlight&idx=' + idxActivateHeaters   + '&switchcmd=Set%20Level&level=110';
    DomoticzJsonTalk( JSON_API );
    JSON_API.path = '/json.htm?type=command&param=switchlight&idx=' + idxUnactivateHeaters + '&switchcmd=Set%20Level&level=110';
    DomoticzJsonTalk( JSON_API );
    
    // Set the DomoticZ Alarm flag if a Heater or Lighting went offline more than one hour ago
    if( myLighting !== 0 )
        if( myLighting++ === 2 ) {
            JSON_API.path = '/json.htm?type=devices&rid=' + idxHeaterFailureFlag;
            DomoticzJsonTalk( JSON_API, RaisefailureFlag );
            myLighting = 0;
        }  // if( myLighting++ === 2 ) {
    myHeaters.forEach(function( value ) {
       if( value.DeviceFault !== 0 )
           if( value.DeviceFault++ === 2 ) {
               JSON_API.path = '/json.htm?type=devices&rid=' + idxHeaterFailureFlag;
               DomoticzJsonTalk( JSON_API, RaisefailureFlag );
               // value.DeviceFault = 0;
           } // if( value.DeviceFault++ === 2 ) {
    }); // myHeaters.forEach(function( value ) {
    
}, PollingTimer*60000); // 