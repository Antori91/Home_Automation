// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// ***** mtCluster:
//        - High Availibilty Active/Passive Domoticz Cluster
//        - Script for the Passive/Backup server *****
// V0.21 - April 2019
          // Improvement : ["mqtt/out", "SelectorSwitch"] using always passcode now (work for protected and non protected selector switches devices)
          // Fire Alarm server (including its Temp sensor) added to the repository
// V0.20 - February 2019
          // Change : Raise alert if backup server not fully operational ONLY after LTIMEOUT heartbeat failures (to avoid false alerts during server maintenance)
          // Improvement : Before starting the Backup DZ Failover, try to restart the Main Domoticz instance using SSH 
          // Improvement : After failure, when Main Domoticz instance is back, stop automatically the Backup DZ Failover (don't have anymore to stop/restart this script)
          // Improvement : If Main Domoticz failure, don't log multiple "Status: DZ FAILOVER HAS BEEN STARTED USING BACKUP SERVER" messages
          // Improvement : SECPANEL synchronization supported
// v0.16 - August 2018
          // NEW FEATURE : Raise alert if backup server not fully operational
          // Reset Heartbeat Timeout Countdown if Domoticz answer
// V0.12  - July 2018 - Initial release
          // NON Failure Condition :
          //        - Connection to Main MQTT server OK and Main Domoticz server Heartbeat OK.
          //          Dz Heartbeat OK is to success to get idxClusterFailureFlag Domoticz device using HTTP/JSON (every n minutes).
          //          Backup server IP address must be in main Domoticz setup : "Local Networks (no username/password)" 
          //        - Synchronize backup Domoticz database with the main one:
          //            - REpublish messages from Main/Mqtt/domoticz/in and Main/Mqtt/domoticz/out topics to Backup/Mqtt/domoticz/in topic
          //            - Domoticz devices to synchronize declared in myIDXtoSync [$$SYNC_REPOSITORY]
          // Only Main DOMOTICZ server Failure Condition :
          //        - Detection : Main Domoticz server Heartbeat failed during TIMEOUT minutes
          //        - Action : Failover with Backup/Mqtt/domoticz/out topic messages published to Main/Mqtt/domoticz/out 
          // Only Main MQTT server Failure Condition :
          //        - Detection : Reconnection to Main MQTT server failed during TIMEOUT minutes 
          //        - Action : Failover with backup server "becoming" the main one using "sudo ifconfig wlan0 MAIN_SERVER_IP"
          //        - Issue : Two Domoticz instances operationnal if Main server not killed using "/json.htm?type=command&param=system_shutdown" - NOT IMPLEMENTED YET
          // MAIN SERVER Down OR Orange LIVEBOX Ethernet LAN Failure Condition :
          //        - Detection : Reconnection to Main MQTT server failed during TIMEOUT minutes 
          //        - Action : Failover with backup server "becoming" the main one using "sudo ifconfig wlan0 MAIN_SERVER_IP"
          // Restarting the service of the main server after a failover
          //        - Restart first this script (and eventually copy the backup Domoticz database to the main server)

const VERBOSE          = false; // Detailed logging or not
const MyJSecretKeys    = require('/home/pi/iot_domoticz/WiFi_DZ_MQTT_SecretKeys.js');
var SSHclient          = require('ssh2').Client;

// SECpanel Status 
const MSECPANEL_DISARM           = 0;    // using MQTT
const MSECPANEL_ARM_HOME         = 11;
const MSECPANEL_ARM_AWAY         = 9;
const JSECPANEL_DISARM           = 0;    // using JSON
const JSECPANEL_ARM_HOME         = 1;
const JSECPANEL_ARM_AWAY         = 2;

// MQTT Cluster Parameters       
const MQTT_ACTIVE_SVR    = 'mqtt://' + MyJSecretKeys.MAIN_SERVER_IP;   
const MQTT_PASSIVE_SVR   = 'mqtt://localhost';
var   ActiveMqtt         = require('mqtt');
var   PassiveMqtt        = require('mqtt');
var   dzFAILURE          = false;   // Main Domoticz server failure flag
var   LdzFAILURE         = false;   // Backup Domoticz server failure flag
var   mdzStatus          = true;    // Main Domoticz server status to log 
var   dzStatus           = true;    // Backup Domoticz server status to log 
var   mqttTROUBLE        = false;   // A MQTT error occured. Main MQTT server failure has to be confirmed
var   mqttFAILURE        = false;   // Main MQTT server failure confirmed
var   LmqttTROUBLE       = false;   // A MQTT error occured. Backup MQTT server failure has to be confirmed
var   LmqttFAILURE       = false;   // Backup MQTT server failure confirmed
const TIMEOUT            = 15;      // Max number of Main server issues (Dz or Mqtt) in a row before we raise Failure signal
const LTIMEOUT           = 2;       // Max number of Backup server issues (Dz or Mqtt) in a row before we raise Failure signal
const HeartbeatTimer     = 1;       // Check Domoticz and MQTT health on both servers every n minutes
var   dzFAILUREcount     = TIMEOUT; // If Main Domoticz failed n times in a row (i.e failure during TIMEOUT * HeartbeatTimer minutes), then trigger Domoticz failover
var   LdzFAILUREcount    = LTIMEOUT;// If Backup Domoticz failed n times in a row (i.e failure during LTIMEOUT * HeartbeatTimer minutes), then alert about cluster not operational
var   mqttFAILUREcount   = TIMEOUT; // If Main Mqtt reconnection didn't happen during this time in minutes (TIMEOUT * HeartbeatTimer), then trigger MQTT failover
var   LmqttFAILUREcount  = LTIMEOUT;// If Backup Mqtt reconnection didn't happen during this time in minutes (LTIMEOUT * HeartbeatTimer), then trigger MQTT failover
var   DZ_STOP            = 'sudo /var/packages/domoticz/scripts/start-stop-status stop\n';   // Synology Platform: scripts to stop/start Domoticz
var   DZ_START           = 'sudo /var/packages/domoticz/scripts/start-stop-status start\n';
if( MyJSecretKeys.MAIN_SERVER_HARDWARE === 'RASPBERRY' ) {
      DZ_STOP            = 'sudo /etc/init.d/domoticz.sh stop\n';   // Raspberry Platform: scripts to stop/start Domoticz
      DZ_START           = 'sudo /etc/init.d/domoticz.sh start\n';
}
const DZ_COMPLETION      = 25000; // Duration in ms to wait for Domoticz shutdown or restart command completion. MUST BE SET LESS THAN HeartbeatTimer/2 
var DZ_RECOVER_SUCCESS   = -1; // -1 = Unknwon, 0 = KO, 1 = OK

function newIP( newIPaddress ) {
  const { spawn } = require('child_process');
  const ifconfig  = spawn('sudo', ['ifconfig', 'wlan0', newIPaddress]);  
  console.log("\n{" + new Date() + " mtCluster-Info] Server: BACKUP, Service: IFCONFIG - WLAN0 IP address CHANGED to " + newIPaddress + ", Status : {");
  ifconfig.stdout.on('data', (data) => { console.log(`stdout: ${data}`); });
  ifconfig.stderr.on('data', (data) => { console.log(`stderr: ${data}`); });
  ifconfig.on('close', (code)       => { console.log(`child process exited with code ${code}`); console.log("}"); });
}  // function newIP( newIPaddress ) {   

function IDXtoSync( IDX, DataSource, DeviceType ) {  // Object to synchronize Domoticz Idx  
    // IDX = Domoticz IDX
    // Datasource : source of the data (currently only MQTT incoming or outcoming messages), "mqtt/in" for activeserver/mqtt/domoticz/in topic or "mqtt/out" for activeserver/mqtt/domoticz/out topic 
    // DeviceType : the Domoticz corresponding device type, currently type only supported are "Light/Switch", "SelectorSwitch", "Temperature" and "Thermostat"  
    this.IDX = IDX; this.DataSource = DataSource; this.DeviceType = DeviceType;
    this.DzSynchronize = function( topic, INsyncmsg ) { 
       if( this.DataSource === "mqtt/in" && topic == 'domoticz/in' )  {  // Just have to republish the message
          PassiveSvr.publish('domoticz/in', INsyncmsg );    
          if( VERBOSE ) console.log("\n[" + new Date() + " mqttCluster-Info] Server: MAIN, Service: SYNC, Incoming message sent to Backup domoticz/in server: " + INsyncmsg);
       } // if( this.DataSource === "mqtt/in" )  { 
       if( this.DataSource === "mqtt/out" && topic == 'domoticz/out' ) {  // OUT MQTT/JSON Domoticz format must be changed to the IN one before republishing   
          var JSONmessage=JSON.parse( INsyncmsg );
          var OUTsyncmsg;           
          if( this.DeviceType === "Light/Switch" )
              if( JSONmessage.nvalue == 0 ) OUTsyncmsg = "{\"command\" : \"switchlight\", \"idx\" : " + this.IDX + ", \"switchcmd\" : \"Off\"}";
              else OUTsyncmsg = "{\"command\" : \"switchlight\", \"idx\" : " + this.IDX + ", \"switchcmd\" : \"On\"}";   
          if( this.DeviceType === "SelectorSwitch" ) OUTsyncmsg = "{\"command\" : \"switchlight\", \"idx\" : " + this.IDX + ", \"switchcmd\" : \"Set Level\", \"level\" : " + JSONmessage.svalue1 + ", \"passcode\" : " + MyJSecretKeys.ProtectedDevicePassword + "}";
          if( this.DeviceType === "Thermostat" ||  this.DeviceType === "Temperature" ) OUTsyncmsg = "{\"command\" : \"udevice\", \"idx\" : " + this.IDX + ", \"nvalue\" : 0, \"svalue\" : \"" + JSONmessage.svalue1 + "\"}";               
          PassiveSvr.publish('domoticz/in', OUTsyncmsg );  
          if( VERBOSE ) console.log("\n[" + new Date() + " mqttCluster-Info] Server: MAIN, Service: SYNC, Outcoming message sent to Backup domoticz/in server: " + OUTsyncmsg);
          if( this.DeviceType === "Secpanel" ) {
              if( JSONmessage.nvalue === MSECPANEL_ARM_HOME ) L_JSON_API.path = '/json.htm?type=command&param=setsecstatus&secstatus=' + JSECPANEL_ARM_HOME + '&seccode=' + MyJSecretKeys.SecPanel_Seccode; 
              if( JSONmessage.nvalue === MSECPANEL_ARM_AWAY ) L_JSON_API.path = '/json.htm?type=command&param=setsecstatus&secstatus=' + JSECPANEL_ARM_AWAY + '&seccode=' + MyJSecretKeys.SecPanel_Seccode; 
              if( JSONmessage.nvalue === MSECPANEL_DISARM )   L_JSON_API.path = '/json.htm?type=command&param=setsecstatus&secstatus=' + JSECPANEL_DISARM   + '&seccode=' + MyJSecretKeys.SecPanel_Seccode; 
              L_JSON_API.path = L_JSON_API.path.replace(/ /g,"");
              LDomoticzJsonTalk( L_JSON_API, LdzStatus );   
              if( VERBOSE ) console.log("\n[" + new Date() + " mqttCluster-Info] Server: MAIN, Service: SYNC, Outcoming SecPanel message sent to Backup domoticz/JSON server: " + OUTsyncmsg);
          } // if( this.DeviceType === "Secpanel" ) {
       } // if( this.DataSource = "mqtt/out" ) {  
    }  // this.DataProcess = function( syncmsg )
}   // function synchronize( IDX, DataSource, DataStore ) {

// Here all Domoticz Idx to synchronize [$$SYNC_REPOSITORY]
var   myIDXtoSync      = [ new IDXtoSync( 50, "mqtt/out", "Light/Switch" ),  new IDXtoSync( 51, "mqtt/out", "Light/Switch" ),  // 50/51=Entree and Mezzanine lighting 
                           new IDXtoSync( MyJSecretKeys.idx_SecPanel, "mqtt/in", "" ), new IDXtoSync( MyJSecretKeys.idx_AlarmALERT, "mqtt/in", "" ),  new IDXtoSync( MyJSecretKeys.idx_AlarmARM, "mqtt/in", "" ),  // Secpanel, Alarm Armed and Alarm Alert flags
                           new IDXtoSync( 34, "mqtt/in", ""),   // 34=Hot Water Tank
                           new IDXtoSync( 27, "mqtt/in", "" ),  new IDXtoSync( 28, "mqtt/in", "" ),   new IDXtoSync( 29, "mqtt/in", ""), new IDXtoSync( 30, "mqtt/in", "" ),   new IDXtoSync( 31, "mqtt/in", ""),   // Ground Floor Heaters
                           new IDXtoSync( 35, "mqtt/in", "" ),  new IDXtoSync( 36, "mqtt/in", "" ),   new IDXtoSync( 37, "mqtt/in", ""), new IDXtoSync( 38, "mqtt/in", "" ),   // First Floor Heaters   
                           new IDXtoSync( MyJSecretKeys.idx_FireAlarmTempHum, "mqtt/in", "" ),  new IDXtoSync( MyJSecretKeys.idx_FireAlarmHeatidx, "mqtt/in", "" ), // Temp sensor of Fire Alarm server
                           new IDXtoSync( MyJSecretKeys.idx_FireALarmStatus, "mqtt/out", "SelectorSwitch" ), // Fire Alarm server status
                           new IDXtoSync( MyJSecretKeys.idx_SecPanel, "mqtt/out", "Secpanel" ), // Secpanel 
                           new IDXtoSync( 17, "mqtt/out", "SelectorSwitch" ), new IDXtoSync( 16, "mqtt/out", "Thermostat" ) ];   //  17=Main heating breaker (OFF/HORSGEL/ECO/CONFORT), 16=Heating thermostat setpoint
                           // heating display/schedule to add to the repository in the future
// [$$SYNC_REPOSITORY]

// ** Domoticz Parameters and communication functions **
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

// *************** HEARTBEAT ROUTINES ***************

setInterval(function(){ // Main Domoticz Heartbeat
   // if( dzFAILURE ) return;
   if( VERBOSE ) console.log("\n[" + new Date() + " mtCluster-Info] Server: MAIN, Service: DOMOTICZ, DzHEARTBEAT TimeToken: " + dzFAILUREcount ); 
   M_JSON_API.path = '/json.htm?type=devices&rid=' + MyJSecretKeys.idxClusterFailureFlag;
   M_JSON_API.path = M_JSON_API.path.replace(/ /g,"");
   MDomoticzJsonTalk( M_JSON_API, dzFailover );   
}, HeartbeatTimer*60000); // 

setInterval(function(){ // Backup Domoticz Heartbeat
   // if( LdzFAILURE ) return;
   if( VERBOSE ) console.log("\n[" + new Date() + " mtCluster-Info] Server: BACKUP, Service: DOMOTICZ, DzHEARTBEAT TimeToken: " + LdzFAILUREcount ); 
   L_JSON_API.path = '/json.htm?type=devices&rid=' + MyJSecretKeys.idxClusterFailureFlag;
   L_JSON_API.path = L_JSON_API.path.replace(/ /g,"");
   LDomoticzJsonTalk( L_JSON_API, LdzStatus );   
}, HeartbeatTimer*60000); // 

var dzFailover   = function( error, data ) { // dzFailover
   if( !error ) {
      DZ_RECOVER_SUCCESS = -1; 
      if( dzFAILURE ) { 
         dzFAILURE = false;
         DZ_RECOVER_SUCCESS = 1;
         console.log("\n[" + new Date() + " mtCluster-Info] Server: MAIN, Service: DOMOTICZ, Status: ON LINE - MANUAL RESTART - DZ FAILOVER HAS BEEN STOPPED");
      }
      else if( mdzStatus || ( dzFAILUREcount > 0 &&  dzFAILUREcount < TIMEOUT ) ) console.log("\n[" + new Date() + " mtCluster-Info] Server: MAIN, Service: DOMOTICZ, Status: ON LINE");
           else if( dzFAILUREcount === 0 ) console.log("\n[" + new Date() + " mtCluster-ALERT] Server: MAIN, Service: DOMOTICZ, Status: ON LINE - AUTOMATIC RESTART HAPPENED");
      mdzStatus = false;
      dzFAILUREcount = TIMEOUT;            
      return;
   } // if( !error ) {  
   if( --dzFAILUREcount === 0 ) {        
          // First, try to restart Domoticz in the main server
          console.log("\n[" + new Date() + " mtCluster-ALERT] Server: MAIN, Service: DOMOTICZ, Status: OFF LINE - TRYING TO RESTART");
          var SSHconn = new SSHclient();
          SSHconn.on('ready', function() {
             console.log("\n[" + new Date() + " mtCluster-Info] Server: MAIN, Service: SSH, Status: Connected");
             SSHconn.shell(function(err, stream) {
                if (err) { console.log("\n[" + new Date() + " mtCluster-FAILURE] Server: MAIN, Service: SSH, Status: Shell Failed"); return; }
                stream.on('close', function() {
                   console.log("\n[" + new Date() + " mtCluster-Info] Server: MAIN, Service: SSH, Status: SSH Session normally Ended");
                   SSHconn.end();
                }).on('data', function(data) {
                   if( VERBOSE ) console.log('STDOUT: ' + data);
                });
                console.log("\n[" + new Date() + " mtCluster-Info] Server: MAIN, Service: SHELL, Status: Stopping Domoticz");
                stream.write(DZ_STOP);
                setTimeout(function(){
                    stream.write( MyJSecretKeys.MAIN_SERVER_ROOT_PWD + '\n');
                    setTimeout(function(){ 
                       console.log("\n[" + new Date() + " mtCluster-Info] Server: MAIN, Service: SHELL, Status: Restarting Domoticz");
                       stream.write(DZ_START);
                       setTimeout(function(){
                          // stream.write( MyJSecretKeys.MAIN_SERVER_ROOT_PWD + '\n');
                          setTimeout(function(){ 
                             stream.end('exit\n');
                          }, DZ_COMPLETION); 
                       }, 1500);
                    }, DZ_COMPLETION); 
                }, 1500);  
             });  // SSHconn.shell
          }).connect({
            host     : MyJSecretKeys.MAIN_SERVER_IP,
            port     : MyJSecretKeys.MAIN_SERVER_SSH_PORT,
            username : MyJSecretKeys.MAIN_SERVER_SSH_USER,
            password : MyJSecretKeys.MAIN_SERVER_ROOT_PWD  
          });  // SSHconn( 'ready' )
          SSHconn.on(  'error', function ()  { 
             console.log("\n[" + new Date() + " mtCluster-FAILURE] Server: MAIN, Service: SSH, Status: Failed to connect");            
          }) // SSHconn.on( 'error' )
   }  //  if( --dzFAILUREcount === 0 ) {        
   if( dzFAILUREcount < 0 ) { 
          if( DZ_RECOVER_SUCCESS === 0 ) return;
          DZ_RECOVER_SUCCESS = 0;
          dzFAILURE = true;
          L_JSON_API.path = '/json.htm?type=devices&rid=' + MyJSecretKeys.idxClusterFailureFlag; // Signal the failure using Domoticz at the backup server
          L_JSON_API.path = L_JSON_API.path.replace(/ /g,"");
          LDomoticzJsonTalk( L_JSON_API, RaisefailureFlag ); 
          if( !mqttFAILURE) console.log("\n[" + new Date() + " mtCluster-FAILURE] Server: MAIN, Service: FAILOVER, Status: DZ FAILOVER HAS BEEN STARTED USING BACKUP SERVER");             
   }  // if( dzFAILUREcount < 0 ) { 
};

var LdzStatus = function( error, data ) { // LdzStatus
   if( !error ) {
      if( LdzFAILURE ) { 
         LdzFAILURE = false;
         console.log("\n[" + new Date() + " mtCluster-Info] Server: BACKUP, Service: DOMOTICZ, Status: ON LINE - DZ HAS BEEN RECOVERED");
      }
      else if( dzStatus || LdzFAILUREcount < LTIMEOUT ) console.log("\n[" + new Date() + " mtCluster-Info] Server: BACKUP, Service: DOMOTICZ, Status: ON LINE");   
      dzStatus = false;
      LdzFAILUREcount = LTIMEOUT;    
      return;
   } // if( !error ) {   
   if( --LdzFAILUREcount === 0 ) {  
      LdzFAILURE = true; 
      dzStatus = true;
      M_JSON_API.path = '/json.htm?type=devices&rid=' + MyJSecretKeys.idxClusterFailureFlag; // Signal the failure using Domoticz at the main server
      M_JSON_API.path = M_JSON_API.path.replace(/ /g,"");
      MDomoticzJsonTalk( M_JSON_API, MRaisefailureFlag );     
      console.log("\n[" + new Date() + " mtCluster-FAILURE] Server: BACKUP, Service: DOMOTICZ, Status: DOWN"); 
   } // if( --ldzFAILUREcount === 0
};

setInterval(function( ) { // Main server Mqtt Heartbeat and failover 
   if( mqttFAILURE ) return;  
   if( VERBOSE ) console.log("\n[" + new Date() + " mtCluster-Info] Server: MAIN, Service: MQTT, MqttHEARTBEAT TimeToken: " + mqttFAILUREcount ); 
   if( mqttTROUBLE ) 
      if( --mqttFAILUREcount <= 0 ) {   
          mqttFAILURE = true;
          if( !LdzFAILURE ) { 
             L_JSON_API.path = '/json.htm?type=devices&rid=' + MyJSecretKeys.idxClusterFailureFlag;
             L_JSON_API.path = L_JSON_API.path.replace(/ /g,"");
             LDomoticzJsonTalk( L_JSON_API, RaisefailureFlag );
          } else {
             M_JSON_API.path = '/json.htm?type=devices&rid=' + MyJSecretKeys.idxClusterFailureFlag;
             M_JSON_API.path = M_JSON_API.path.replace(/ /g,"");
             MDomoticzJsonTalk( M_JSON_API, MRaisefailureFlag );
          }  // if( !LdzFAILURE ) { 
          // Kill Main Domoticz server - NOT IMPLEMENTED YET : we assume MQTT went down because the main server went down 
          // M_JSON_API.path = '/json.htm?type=command&param=system_shutdown';
          // M_JSON_API.path = M_JSON_API.path.replace(/ /g,"");
          // MDomoticzJsonTalk( M_JSON_API ); 
          // Wait 5s to be sure all current (HTTP) requests complete. Then Stop MQTT here and become the main server ! 
          setTimeout(function( ){
             ActiveSvr.end(true);
             PassiveSvr.end(true);
             newIP( MyJSecretKeys.MAIN_SERVER_IP );
             console.log("\n[" + new Date() + " mtCluster-FAILURE] Server: MAIN, Service: FAILOVER, Status: DZ AND MQTT FAILOVER HAS BEEN STARTED USING BACKUP SERVER");
          }, 5000 );
      } // if( --mqttFAILUREcount <= 0 ) {
}, HeartbeatTimer*60000); //

setInterval(function( ) { // Backup server Mqtt Heartbeat
   if( LmqttFAILURE ) return;  
   if( VERBOSE ) console.log("\n[" + new Date() + " mtCluster-Info] Server: BACKUP, Service: MQTT, MqttHEARTBEAT TimeToken: " + LmqttFAILUREcount ); 
   if( LmqttTROUBLE ) 
      if( --LmqttFAILUREcount <= 0 ) {   
          LmqttFAILURE = true;
          console.log("\n[" + new Date() + " mtCluster-FAILURE] Server: BACKUP, Service: MQTT, Status: DOWN" ); 
          if( !LdzFAILURE ) { 
             L_JSON_API.path = '/json.htm?type=devices&rid=' + MyJSecretKeys.idxClusterFailureFlag;
             L_JSON_API.path = L_JSON_API.path.replace(/ /g,"");
             LDomoticzJsonTalk( L_JSON_API, RaisefailureFlag );
          } else {
             M_JSON_API.path = '/json.htm?type=devices&rid=' + MyJSecretKeys.idxClusterFailureFlag;
             M_JSON_API.path = M_JSON_API.path.replace(/ /g,"");
             MDomoticzJsonTalk( M_JSON_API, MRaisefailureFlag );
          }  // if( !LdzFAILURE ) { 
      } // if( --LmqttFAILUREcount <= 0 ) { 
}, HeartbeatTimer*60000); //

// *************** SUBROUTINES TO TALK TO DOMOTICZ  ***************

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
         if( dzFAILUREcount === TIMEOUT || VERBOSE ) console.log("\n[" + new Date() + " mtCluster-ALERT] Server: MAIN, Service: DOMOTICZ, Status: " + e.message + " - Can't reach DomoticZ with URL: " + savedURL );     
         if( callBack ) callBack( e, null, objectToCompute );
   });
};   // function MDomoticzJsonTalk( JsonUrl ) 

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
         if( LdzFAILUREcount === LTIMEOUT || VERBOSE ) console.log("\n[" + new Date() + " mtCluster-ALERT] Server: BACKUP, Service: DOMOTICZ, Status: " + e.message + " - Can't reach DomoticZ with URL: " + savedURL );     
         if( callBack ) callBack( e, null, objectToCompute );
   });
};   // function LDomoticzJsonTalk( JsonUrl ) 

var RaisefailureFlag = function( error, data ) { // Raise failure flag using Backup Domoticz server
  if( error ) return;
  if( data.result[0].Status === "Off" ) { // to avoid to signal many times (w/ many sms/emails) that the main server is in trouble
       L_JSON_API.path = '/json.htm?type=command&param=switchlight&idx=' + MyJSecretKeys.idxClusterFailureFlag + '&switchcmd=On&passcode=' + MyJSecretKeys.ProtectedDevicePassword;
       L_JSON_API.path = L_JSON_API.path.replace(/ /g,"");
       LDomoticzJsonTalk( L_JSON_API );                
  }  // if( GetAlarmIDX.result[0].Status === 
}; // var RaisefailureFlag = fu

var MRaisefailureFlag = function( error, data ) { // Raise failure flag using Main Domoticz server
  if( error ) return;
  if( data.result[0].Status === "Off" ) { // to avoid to signal many times (w/ many sms/emails) that the main server is in trouble
       M_JSON_API.path = '/json.htm?type=command&param=switchlight&idx=' + MyJSecretKeys.idxClusterFailureFlag + '&switchcmd=On&passcode=' + MyJSecretKeys.ProtectedDevicePassword;
       M_JSON_API.path = M_JSON_API.path.replace(/ /g,"");
       MDomoticzJsonTalk( M_JSON_API );                
  }  // if( GetAlarmIDX.result[0].Status === 
}; // var MRaisefailureFlag = fu

// *************** MAIN START HERE ***************
console.log("\n*** " + new Date() + " - mtCluster V0.21 High Availability Domoticz Cluster starting ***");
console.log("mtCluster MQTT servers hardware  = " +  MyJSecretKeys.MAIN_SERVER_HARDWARE + " - " + MyJSecretKeys.BACKUP_SERVER_HARDWARE);
console.log("mtCluster MQTT nodes address     = " +  MQTT_ACTIVE_SVR + " - " + MQTT_PASSIVE_SVR);
console.log("mtCluster Domoticz nodes address = " +  M_JSON_API.host + ":" + M_JSON_API.port + " - " + L_JSON_API.host + ":" + L_JSON_API.port);

// Reset backup server IP address to default one
newIP( MyJSecretKeys.BACKUP_SERVER_IP );                     

// Start MQTT and then manage events
var ActiveSvr   = ActiveMqtt.connect( MQTT_ACTIVE_SVR   );   
var PassiveSvr  = PassiveMqtt.connect( MQTT_PASSIVE_SVR );   

// MQTT Error events
ActiveSvr.on(  'error', function ()  { 
   console.log("\n[" + new Date() + " mtCluster-ALERT] Server: MAIN, Service: MQTT, Status: CAN'T CONNECT to MQTT at boot");
   mqttTROUBLE = true; })     // Emitted when the client cannot connect (i.e. connack rc != 0) 
PassiveSvr.on( 'error', function ()  {
   console.log("\n[" + new Date() + " mtCluster-ALERT] Server: BACKUP, Service: MQTT, Status: CAN'T CONNECT to MQTT at boot"); 
   LmqttTROUBLE = true; })

ActiveSvr.on(  'close', function ()  { 
   if( VERBOSE ) console.log("\n[" + new Date() + " mtCluster-ALERT] Server: MAIN, Service: MQTT, Status: DOWN");
   mqttTROUBLE = true; })     // Emitted after a disconnection  
PassiveSvr.on( 'close', function ()  { 
   if( VERBOSE ) console.log("\n[" + new Date() + " mtCluster-ALERT] Server: BACKUP, Service: MQTT, Status: DOWN");
   LmqttTROUBLE = true; })
ActiveSvr.on(  'offline', function ()  { 
   console.log("\n[" + new Date() + " mtCluster-ALERT] Server: MAIN, Service: MQTT, Status: OFF LINE")
   mqttTROUBLE = true; })   // Emitted when the client goes offline 
PassiveSvr.on( 'offline', function ()  {  
   console.log("\n[" + new Date() + " mtCluster-ALERT] Server: BACKUP, Service: MQTT, Status: OFF LINE");
   LmqttTROUBLE = true; })

ActiveSvr.on(  'reconnect', function ()  { 
   if( VERBOSE ) console.log("\n[" + new Date() + " mtCluster-Info] Server: MAIN, Service: MQTT, Status: Trying to reconnect");
   mqttTROUBLE = true; }) // Emitted when a reconnect starts
PassiveSvr.on( 'reconnect', function ()  { 
   if( VERBOSE ) console.log("\n[" + new Date() + " mtCluster-Info] Server: BACKUP, Service: MQTT, Status: Trying to reconnect"); 
   LmqttTROUBLE = true; })

// MQTT Subscribe to topics on Connect event
ActiveSvr.on(  'connect', function ()  { 
   if( !mqttFAILURE ) {
      console.log("\n[" + new Date() + " mtCluster-Info] Server: MAIN, Service: MQTT, Status: ON LINE");
      mqttTROUBLE = false; mqttFAILUREcount = TIMEOUT;
      ActiveSvr.subscribe( ['domoticz/in', 'domoticz/out'] );
   } })  
PassiveSvr.on( 'connect', function ()  { 
   console.log("\n[" + new Date() + " mtCluster-Info] Server: BACKUP, Service: MQTT, Status: ON LINE");
   LmqttTROUBLE = false; LmqttFAILUREcount = LTIMEOUT;
   PassiveSvr.subscribe( ['domoticz/out']  ); })  

// MQTT Messages received events
ActiveSvr.on('message', function (topic, message) {   
     if( mqttFAILURE ) return;
     var JSONmessage=JSON.parse(message);         
     if( dzFAILURE ) { 
        if( topic == 'domoticz/in' ) {  // Domoticz failover - We have to send every incoming messages to the backup Domoticz server
            PassiveSvr.publish('domoticz/in', message ); 
            if( VERBOSE ) console.log("\n[" + new Date() + " mtCluster-Failover] Server: MAIN, Service: FAILOVER, Message from domoticz/in DUPLICATED TO BACKUP server: " + message.toString() );
        } if( VERBOSE ) console.log("\n[" + new Date() + " mtCluster-Failover] Server: MAIN, Service: FAILOVER, Message IGNORED from domoticz/out: " + message.toString() );
     } else {  
        // Cluster Normal Condition here
        // The reason to filter and so not republish all incoming messages is mainly for some actuators declared as outcoming message device in the repository, whose internal state changes by a Domoticz command or not. 
        // If state change decided locally to the device, we would have an incoming message and an outgoing message, hence two lines in the corresponding Domoticz device log
        // Can be changed in the future to avoid any incoming message device declaration in the repository
        if( VERBOSE ) console.log("\n[" + new Date() + " mtCluster-Info] Server: MAIN, Service: SYNC, Checking this message from " + topic.toString() + ": " + message.toString() ); 
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
     if( VERBOSE ) console.log("\n[" + new Date() + " mtCluster-Failover] Server: BACKUP, Service: FAILOVER, Message from domoticz/out DUPLICATED TO MAIN server: " + message);       
}) // PassiveSvr.on('message', funct




