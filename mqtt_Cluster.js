// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// ***** Domoticz High Availibilty Active/Passive Cluster - Script for the Passive/Backup server *****
// V0.12 - July 2018 - Initial release
          // NON Failure Condition :
          //        - Heartbeat OK is to success to get idxClusterFailureFlag Domoticz device using HTTP/JSON (every n minutes). Your backup server IP address must be in your main Domoticz setup : "Local Networks (no username/password)" 
          //        - Synchronize backup Domoticz database with the main one, Get ActiveMqtt/domoticz/in and ActiveMqtt/domoticz/out topics messages and published them to PassiveMqtt/domoticz/in topic
          //        - Domoticz devices to synchronize declared in myIDXtoSync [$$SYNC_REPOSITORY]
          // Only Main DOMOTICZ FAILURE Condition :
          //        - Trigger : Hearbeat failed during TIMEOUT minutes
          //        - Failover with Backup/Mqtt/domoticz/out topic messages published to Main/Mqtt/domoticz/out 
          // Main server or MQTT FAILURE Condition :
          //        - Trigger : Main/MQTT reconnection failed during TIMEOUT minutes 
          //        - Failover with backup server "becoming" the main one "sudo ifconfig wlan0 MAIN_SERVER_IP"
          // Restarting the service of the main server after a failover
          //        - Restart first this script (and eventually copy the backup Domoticz database to the main server)

const VERBOSE        = false; // logging verbose or not
const MyJSecretKeys  = require('/home/pi/iot_domoticz/WiFi_DZ_MQTT_SecretKeys.js');
const child          = require('child_process');
const execSync       = child.execSync;

// MQTT Cluster Parameters       
const MQTT_ACTIVE_SVR    = 'mqtt://' + MyJSecretKeys.MAIN_SERVER_IP;   
const MQTT_PASSIVE_SVR   = 'mqtt://localhost';
var   ActiveMqtt         = require('mqtt');
var   PassiveMqtt        = require('mqtt');
var   dzFAILURE          = false;
var   mqttTROUBLE        = false;  // An error occured. MQTT failure has to be confirmed
var   mqttFAILURE        = false;  // MQTT failure confirmed
const TIMEOUT            = 15;
const HearbeatTimer      = 1; // Check Main Domoticz/MQTT health every n minutes
var   dzFAILUREcount     = TIMEOUT; // If Main Domoticz failed n times in a row (i.e failure during TIMEOUT * HearbeatTimer minutes), then trigger Domoticz failover
var   mqttFAILUREcount   = TIMEOUT; // If Main MQTT reconnection didn't happen during this time in minutes (TIMEOUT * HearbeatTimer), then trigger MQTT failover
var   commandLine;
execSync(commandLine, (err, stdout, stderr) => {
  if (err) {
    // node couldn't execute the command
    console.log("\n== " + new Date() + " - CLUSTER GENERAL FAILURE - Failed to change BACKUP SERVER WLAN0 IP address" );
    return;
  }
  if( VERBOSE ) console.log(`stdout: ${stdout}`);
  if( VERBOSE ) console.log(`stderr: ${stderr}`);
});  
var newIP = function( newIPaddress) {
  commandLine = 'sudo ifconfig wlan0 ' + newIPaddress;
  execSync( commandLine );
  console.log("\n== " + new Date() + " - BACKUP SERVER - WLAN0 IP address changed to " + newIPaddress );
}     

function IDXtoSync( IDX, DataSource, DeviceType ) {  // Object to synchronize Domoticz Idx  
    // IDX = Domoticz IDX
    // Datasource : source of the data (currently only MQTT incoming or outcoming messages), "mqtt/in" for activeserver/mqtt/domoticz/in topic or "mqtt/out" for activeserver/mqtt/domoticz/out topic 
    // DeviceType : the Domoticz corresponding device type, currently type only supported are "Light/Switch", "SelectorSwitch", "Temperature" and "Thermostat"  
    this.IDX = IDX; this.DataSource = DataSource; this.DeviceType = DeviceType;
    this.DzSynchronize = function( topic, INsyncmsg ) { 
       if( this.DataSource === "mqtt/in" && topic == 'domoticz/in' )  {  // Just have to republish the message
          PassiveSvr.publish('domoticz/in', INsyncmsg );    
          if( VERBOSE ) console.log("\n*** CLUSTER NORMAL CONDITION - Incoming message sent to Backup Mqtt/Domoticz/in server: " + INsyncmsg);
       } // if( this.DataSource === "mqtt/in" )  { 
       if( this.DataSource === "mqtt/out" && topic == 'domoticz/out' ) {  // OUT MQTT/JSON Domoticz format must be changed to the IN one before republishing   
          var JSONmessage=JSON.parse( INsyncmsg );
          var OUTsyncmsg;           
          if( this.DeviceType === "Light/Switch" )
              if( JSONmessage.nvalue == 0 ) OUTsyncmsg = "{\"command\" : \"switchlight\", \"idx\" : " + this.IDX + ", \"switchcmd\" : \"Off\"}";
              else OUTsyncmsg = "{\"command\" : \"switchlight\", \"idx\" : " + this.IDX + ", \"switchcmd\" : \"On\"}";   
          if( this.DeviceType === "SelectorSwitch" ) OUTsyncmsg = "{\"command\" : \"switchlight\", \"idx\" : " + this.IDX + ", \"switchcmd\" : \"Set Level\", \"level\" : " + JSONmessage.svalue1 + "}";
          if( this.DeviceType === "Thermostat" ||  this.DeviceType === "Temperature" ) OUTsyncmsg = "{\"command\" : \"udevice\", \"idx\" : " + this.IDX + ", \"nvalue\" : 0, \"svalue\" : \"" + JSONmessage.svalue1 + "\"}";               
          PassiveSvr.publish('domoticz/in', OUTsyncmsg );  
          if( VERBOSE ) console.log("\n*** CLUSTER NORMAL CONDITION - Outcoming message sent to Backup Mqtt/Domoticz/in server: " + OUTsyncmsg);
       } // if( this.DataSource = "mqtt/out" ) {  
    }  // this.DataProcess = function( syncmsg )
}   // function synchronize( IDX, DataSource, DataStore ) {

// Here all Domoticz Idx to synchronize [$$SYNC_REPOSITORY]
var   myIDXtoSync      = [ new IDXtoSync( 50, "mqtt/out", "Light/Switch" ),  new IDXtoSync( 51, "mqtt/out", "Light/Switch" ),   new IDXtoSync( 34, "mqtt/in", ""),   // 50/51=Entree and Mezzanine lighting, 34=Hot Water Tank
                           new IDXtoSync( 27, "mqtt/in", "" ),  new IDXtoSync( 28, "mqtt/in", "" ),   new IDXtoSync( 29, "mqtt/in", ""), new IDXtoSync( 30, "mqtt/in", "" ),   new IDXtoSync( 31, "mqtt/in", ""),   // Ground Floor Heaters
                           new IDXtoSync( 35, "mqtt/in", "" ),  new IDXtoSync( 36, "mqtt/in", "" ),   new IDXtoSync( 37, "mqtt/in", ""), new IDXtoSync( 38, "mqtt/in", "" ),   // First Floor Heaters   
                           new IDXtoSync( 17, "mqtt/out", "SelectorSwitch" ), new IDXtoSync( 16, "mqtt/out", "Thermostat" ) ];   //  17=Main OFF/HORSGEL/ECO/CONFORT heating breaker, 16=Heating thermostat setpoint
                           // heating display/schedule to add to the repository in the future
// [$$SYNC_REPOSITORY]

var http_MDomoticz = require('http');  // To access Domoticz in the main server using HTTP/JSON
var M_JSON_API     = {
host: MyJSecretKeys.MAIN_SERVER_IP,         
port: MyJSecretKeys.DZ_PORT,                           
path: '/'
};
var http_LDomoticz = require('http'); // To access Domoticz in the local/backup server using HTTP/JSON
var L_JSON_API     = {
host: 'localhost',         
port: MyJSecretKeys.DZ_PORT,                            
path: '/'
};

setInterval(function(){ // dzHeartbeat
   if( dzFAILURE ) return;
   if( VERBOSE ) console.log("\n** " + new Date() + " - CLUSTER DZ Heartbeat - TimeToken=" + dzFAILUREcount ); 
   M_JSON_API.path = '/json.htm?type=devices&rid=' + MyJSecretKeys.idxClusterFailureFlag;
   M_JSON_API.path = M_JSON_API.path.replace(/ /g,"");
   MDomoticzJsonTalk( M_JSON_API, dzFailover );   
}, HearbeatTimer*60000); //  

var dzFailover   = function( error, data ) { // dzFailover
   if( !error ) return;
   if( --dzFAILUREcount <= 0 ) {   
      dzFAILURE = true;
      L_JSON_API.path = '/json.htm?type=devices&rid=' + MyJSecretKeys.idxClusterFailureFlag; // Signal the failure using Domoticz at the backup server
      L_JSON_API.path = L_JSON_API.path.replace(/ /g,"");
      LDomoticzJsonTalk( L_JSON_API, RaisefailureFlag ); 
      if( !mqttFAILURE) console.log("\n== " + new Date() + " - MAIN DOMOTICZ SERVER FAILURE - DOMOTICZ FAILOVER HAS BEEN STARTED ==" ); 
   }
};

setInterval(function( ) { // mqttHeartbeat and failover 
   if( mqttFAILURE ) return;  
   if( VERBOSE ) console.log("\n** " + new Date() + " - CLUSTER MQTT Heartbeat - TimeToken=" + mqttFAILUREcount );   
   if( mqttTROUBLE ) 
      if( --mqttFAILUREcount <= 0 ) {   
          mqttFAILURE = true;
          L_JSON_API.path = '/json.htm?type=devices&rid=' + MyJSecretKeys.idxClusterFailureFlag; // Signal the failure using Domoticz at the backup server
          L_JSON_API.path = L_JSON_API.path.replace(/ /g,"");
          LDomoticzJsonTalk( L_JSON_API, RaisefailureFlag ); 
          // Kill Main Domoticz server - NOT IMPLEMENTED YET : we assume MQTT went down because the main server went down 
          // M_JSON_API.path = '/json.htm?type=command&param=system_shutdown';
          // M_JSON_API.path = M_JSON_API.path.replace(/ /g,"");
          // MDomoticzJsonTalk( M_JSON_API ); 
          // Wait 5s to be sure all current (HTTP) requests complete. Then Stop MQTT here and become the main server ! 
          setTimeout(function( ){
             ActiveSvr.end(true);
             PassiveSvr.end(true);
             newIP( MyJSecretKeys.MAIN_SERVER_IP );
             console.log("\n== " + new Date() + " - GENERAL MAIN SERVER FAILURE - MQTT AND DOMOTICZ FAILOVER HAS BEEN STARTED ==" );
          }, 5000 );
      } // if( mqttFAILUREcount-- < 0 ) {   
}, HearbeatTimer*60000); //

// Function to talk to MAIN DomoticZ. Do Callback if any 
var MDomoticzJsonTalk = function( JsonUrl, callBack, objectToCompute ) {    
   var savedURL=JSON.stringify(JsonUrl);
   if( VERBOSE ) console.log("\n** Main DomoticZ URL request=" + savedURL );
   http_MDomoticz.get(JsonUrl, function(resp){
      var HttpAnswer = "";
      resp.on('data', function(ReturnData){ 
            HttpAnswer += ReturnData;
      });
      resp.on('end', function(ReturnData){ 
         if( VERBOSE ) console.log("\nMain DomoticZ answer=" + HttpAnswer);
         if( callBack ) callBack( null, JSON.parse(HttpAnswer), objectToCompute );
      });
   }).on("error", function(e){
         console.log("\n== " + new Date() + " - MAIN SERVER TROUBLE - " + e.message + " - Can't reach Main DomoticZ server with URL: " + savedURL );     
         if( callBack ) callBack( e, null, objectToCompute );
   });
};   // function DomoticzJsonTalk( JsonUrl ) 

// Function to talk to LOCAL/BACKUP DomoticZ. Do Callback if any 
var LDomoticzJsonTalk = function( JsonUrl, callBack, objectToCompute ) {    
   var savedURL=JSON.stringify(JsonUrl);
   if( VERBOSE ) console.log("\n** Backup DomoticZ URL request=" + savedURL );
   http_LDomoticz.get(JsonUrl, function(resp){
      var HttpAnswer = "";
      resp.on('data', function(ReturnData){ 
            HttpAnswer += ReturnData;
      });
      resp.on('end', function(ReturnData){ 
         if( VERBOSE ) console.log("\nBackup DomoticZ answer=" + HttpAnswer);
         if( callBack ) callBack( null, JSON.parse(HttpAnswer), objectToCompute );
      });
   }).on("error", function(e){
         console.log("\n== " + new Date() + " - CLUSTER GENERAL FAILURE - " + e.message + " - Can't reach Backup DomoticZ server with URL: " + savedURL );     
         if( callBack ) callBack( e, null, objectToCompute );
   });
};   // function DomoticzJsonTalk( JsonUrl ) 

var RaisefailureFlag = function( error, data ) {
  if( error ) return;
  if( data.result[0].Status === "Off" ) { // to avoid to signal many times (w/ many sms/emails) that the main server is in trouble
       L_JSON_API.path = '/json.htm?type=command&param=switchlight&idx=' + MyJSecretKeys.idxClusterFailureFlag + '&switchcmd=On&passcode=' + MyJSecretKeys.ProtectedDevicePassword;
       L_JSON_API.path = L_JSON_API.path.replace(/ /g,"");
       LDomoticzJsonTalk( L_JSON_API );                
  }  // if( GetAlarmIDX.result[0].Status === 
}; // var RaisefailureFlag = fu

// *************** MAIN START HERE ***************
console.log("\n*** " + new Date() + " - High Availability Domoticz Cluster started ***");
console.log("Cluster MQTT address= " +  MQTT_ACTIVE_SVR + " - " + MQTT_PASSIVE_SVR);
console.log("Cluster Domoticz address= " +  M_JSON_API.host + ":" + M_JSON_API.port + " - " + L_JSON_API.host + ":" + L_JSON_API.port);

// Reset IP address to backup server default one
newIP( MyJSecretKeys.BACKUP_SERVER_IP );                     

// Start MQTT and then manage events
var ActiveSvr   = ActiveMqtt.connect( MQTT_ACTIVE_SVR   );   
var PassiveSvr  = PassiveMqtt.connect( MQTT_PASSIVE_SVR );   

// MQTT Error events
ActiveSvr.on(  'error', function ()  { 
   if( VERBOSE ) console.log("\n== " + new Date() + " - MAIN SERVER TROUBLE - Can't connect to Mqtt at boot time ==");
   mqttTROUBLE = true; })     // Emitted when the client cannot connect (i.e. connack rc != 0) 
// PassiveSvr.on( 'error', function ()  {  })
ActiveSvr.on(  'close', function ()  { 
   if( VERBOSE ) console.log("\n== " + new Date() + " - MAIN SERVER TROUBLE - MQTT DOWN ==");
   mqttTROUBLE = true; })     // Emitted after a disconnection  
// PassiveSvr.on( 'close', function ()  {  })
ActiveSvr.on(  'offline', function ()  { 
   console.log("\n== " + new Date() + " - MAIN SERVER TROUBLE - MQTT OFF LINE ==");
   mqttTROUBLE = true; })   // Emitted when the client goes offline 
// PassiveSvr.on( 'offline', function ()  {  })
ActiveSvr.on(  'reconnect', function ()  { 
   if( VERBOSE ) console.log("\n== " + new Date() + " - MAIN SERVER TROUBLE - Trying to reconnect to Mqtt ==");
   mqttTROUBLE = true; }) // Emitted when a reconnect starts
// PassiveSvr.on( 'reconnect', function ()  {  })

// MQTT Subscribe to topics on Connect event
ActiveSvr.on(  'connect', function ()  { 
   if( !mqttFAILURE ) {
      console.log("\n== " + new Date() + " - MAIN SERVER NORMAL CONDITION - MQTT ON LINE ==");
      mqttTROUBLE = false; mqttFAILUREcount = TIMEOUT;
      ActiveSvr.subscribe( ['domoticz/in', 'domoticz/out'] );
   } })  
PassiveSvr.on( 'connect', function ()  { 
   console.log("\n== " + new Date() + " - BACKUP SERVER NORMAL CONDITION - MQTT ON LINE ==");
   PassiveSvr.subscribe( ['domoticz/out']  ); })  

// MQTT Messages received events
ActiveSvr.on('message', function (topic, message) {   
     if( mqttFAILURE ) return;
     var JSONmessage=JSON.parse(message);         
     if( dzFAILURE ) { 
        if( topic == 'domoticz/in' ) {  // Domoticz failover - We have to send every incoming messages to the backup Domoticz server
            PassiveSvr.publish('domoticz/in', message ); 
            if( VERBOSE ) console.log("\n*** MAIN SERVER DOMOTICZ FAILOVER - Incoming message sent to Backup Mqtt/Domoticz/in server: " + message.toString());
        } if( VERBOSE ) console.log("\n== " + new Date() + " - MAIN SERVER DOMOTICZ FAILOVER - Ignored Outcoming Message from Main Mqtt server: " + message.toString() )
     } else {  
        // Cluster Normal Condition here
        // The reason to filter and so not republish all incoming messages is mainly for some actuators declared as outcoming message device in the repository, whose internal state changes by a Domoticz command or not. 
        // If state change decided locally to the device, we would have an incoming message and an outgoing message, hence two lines in the corresponding Domoticz device log
        // Can be changed in the future to avoid any incoming message device declaration in the repository
        if( VERBOSE ) console.log("\n== " + new Date() + " - CLUSTER NORMAL CONDITION - Message from Main Mqtt server, Topic: " + topic.toString() + ", Message: " + message.toString() ); 
        var IDXsmsg =  JSONmessage.idx;
        myIDXtoSync.forEach(function( value ) {
          if ( IDXsmsg == value.IDX ) value.DzSynchronize( topic, message );                   
        });    
     } //  if( dzFAILURE ) {
}) // ActiveSvr.on('message', funct
PassiveSvr.on('message', function (topic, message) {  // This one used for Domoticz Failover
     if( !dzFAILURE || mqttFAILURE ) return;
     var JSONmessage=JSON.parse(message);
     message=message.toString();
     ActiveSvr.publish('domoticz/out', message );  
     if( VERBOSE ) console.log("\n== " + new Date() + " - MAIN SERVER DOMOTICZ FAILOVER - Outcoming Backup Mqtt/Domoticz/out server message sent to Main Mqtt server: " + message);       
}) // PassiveSvr.on('message', funct



