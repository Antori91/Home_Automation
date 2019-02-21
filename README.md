# Home Automation DIY project using Domoticz, motionEye and ESP8266

## Main achievements:
  - Smart electrical heating management using existing basic heaters
  - Detailed energy monitoring. House thermal characteristics computation
  - Legacy wired alarm appliance go to Internet
  - Cameras and motion detection with motionEye. End to end communication between motionEye and Domoticz 
  - Ligthing management reusing existing latching relays and wall pushbuttons
  - Robust implementation with two Domoticz synchronized servers (failover cluster) and alerting on sensors/actuators failure 

## Architecture:
  - Domoticz High Availability Cluster : Synology Dz V3.5877 (Main) - Raspberry Dz V4.97 (Backup) - Node.js scripts
  - Alarm server : Raspberry - motionEye - iot_ALARM-SVR Node.js script
  - Temperature Sensors and Lighting/Heaters Actuators : ESP8266 (Standalone ESP-12E and Electrodragon IoT ESP8266 Relay Board) - Arduino sketches
  - Communication protocol : MQTT

## Forum topics about:
  - http://www.domoticz.com/forum/viewtopic.php?f=38&t=17032
  
  - http://www.domoticz.com/forum/viewtopic.php?f=38&t=24088

  - http://www.domoticz.com/forum/viewtopic.php?f=38&t=21608

  - http://www.domoticz.com/forum/viewtopic.php?f=38&t=22436

  - http://www.domoticz.com/forum/viewtopic.php?f=38&t=23914

  - https://easydomoticz.com/forum/viewtopic.php?f=21&t=4798

## How to built the corresponding environment:
  - Setup three servers. Synology: main Domoticz server, Raspberry#1: backup Domoticz server and Raspberry#2: dedicated Alarm server. Mandatory software:
     - MQTT for the two Domoticz servers,
     - Node.js for all three (version greater than 6 for the two Raspberry) with packages mqtt (all), ssh2 (Raspberry#1), epoll and rpi-gpio (Raspberry#2)    
  - Install motionEye in the dedicated Alarm server. Update the /etc/motioneye/motion.conf file to have the following lines at the end of the file: webcontrol_html_output on, webcontrol_port XXXX where XXXX is the port number choosen (7999 by default), setup_mode off, webcontrol_parms 2 and webcontrol_localhost off
  - Install Domoticz in the main server:
     - Hardware: 
         - "Dummy (Does nothing, use for virtual switches only)"
         - "MQTT Client Gateway with LAN interface" with Remote address=localhost, port=1883 and Publish Topic=out
     - Cameras: add all cameras
     - Devices (Except Security Panel, all are Dummy Virtual sensors): 
         - "Security Panel"
         - Two protected "Light/Switch Switch" for Alarm status (Motion Sensor subtype) and Home Intrusion Alert (On/Off subtype). Define On/Off actions and Notification to send email/sms (arm/disarm confirmation messages and intrusion alert message). To send SMS, I use Web REST API of the french FREE Telco operator 
         - "Light/Switch Switch" for each (PTZ or non PTZ) camera and one additional "Light/Switch Selector Switch" for each PTZ camera. For each "Light/Switch Switch" camera, define On/Off actions to activate/disactivate motionEye motion detection for this camera. For "Light/Switch Selector Switch", define the level names and actions according to the PTZ Camera setpoint commands
         - "Temp + Humidity" for each Temperature sensor and one for Degrees.Days
         - one "Light/Switch Switch" for Electricity tariff
         - "P1 Smart Meter" for each heater and one for overall heating index meter 
         - One "Light/Switch Selector switch" for Heating main breaker. Define the levels OFF/HORSGEL/ECO/CONFORT 
         - Two "Light/Switch Selector switches" for Heating Schedule/Start and Heating Schedule/Stop. Define the levels according to the heating zones
         - one "General	Text" to display the heating zones status
         - one "Thermostat SetPoint"
         - two "General Custom Sensors" for Thermal loss and Cooling rate
         - "Light/Switch Switch" for each lighting zone
         - Three "Light/Switch Switch/Smoke Detector" for Failure status regarding Temperature sensors, Lighting server and Alarm server. Define On action and Notification to send email/sms if failure 
     - User variables: 
         - Idx=1, "String" and name "HeatersActive" for Heaters status. Initial value: {"command" : "activateheaters", "28" : "On", "29" : "On", .... , "34" : "On"} where 28, 29 .... 34 are Heater IDX number
         - "Integer" and name "TWILIGHTimer0" for Lighting timer. Initial value: 0  
         - "Integer" and name "MezzaOverHeated" for one of heating cost optimizer rule. Initial value: 0 
     - Blockly: enter the blockly according to the GIF images given
     - Scripts: 
         - Copy to the installation directory the nodejs and shell scripts. By default, this installation directory is "/volume1/@appstore/iot_domoticz" (Synology) and "/home/pi/iot_domoticz" (Raspberry)
         - Update the WiFi_DZ_MQTT_SecretKeys.js file according to the environment
         - Update myHeaters repository at line 133 of iot_Orchestrator.js file according to heating zones and heaters per heating zone used. Update also eventually lines 215 and 226 regarding Heating Schedule/Start and Heating Schedule/Stop actual names choosen
         - Setup Crontab to launch the shell scripts at boot
     - Security setup: Local Networks (no username/password) set to accept connections without authentication from the Backup server and from "localhost;127.0.0.*"      
  - Backup the Domoticz database in the main server
  - Install Domoticz in the backup server:
     - Devices: use the main Domoticz database to setup again the devices
     - Blockly: enter again the blockly
     - Scripts: copy again all the (modified) scripts and setup again Crontab
  -  Install the Cluster feature :
     - Copy the cluster scripts (nodejs and shell) to the backup server
     - Update myIDXtoSync repository at line 118 of mqtt_Cluster.js file according to the devices to synchronize 
     - Setup Crontab to run it at boot
  - Install the Alarm scripts (nodejs and shell) in the dedicated Alarm server
  - Setup all the ESP8266:
     - Using Arduino IDE (Files/Preferences/Additional Board Manager set to http://arduino.esp8266.com/versions/2.3.0/package_esp8266com_index.json):
         - Update WiFi_OTA_MQTT_SecretKeys.h according to the environment
         - Update pubsub.h
         - Update heaters repository from lines 130 to 166 of iot_ESP8266_AC712.ino file
         - Update lighting repository and various parameters from lines 36 to 74 of iot_ESP8266_GM43.ino file
         - Update temperature sensors repository and various parameters from lines 57 to 62 of iot_ESP8266_DHT22.ino file
         - Compile the sketches and flash the ESP8266 
     - Install the ESP8266 and connect them to the devices (heaters, hot water tank, lighting relays)
  - Arrived here, time to play with Domoticz...Enter for the main and backup Domoticz instances the heating schedules per heating zone. For the backup server, I've entered schedules to send every hour a command to start all heating zones. In my environment, there is no synchronization between the heating schedules of the main and backup servers. To start or stop a heating zone at a given hour, you have to enter in Timers of Heating Schedule/Start or Heating Schedule/Stop devices the command ON on Time for the level corresponding to the heating zone  
           

           
