# Home Automation DIY project using ESP8266-Arduino, Node.js, Domoticz and motionEye 
Powerful and low price sensors/actuators. Tailored solution leveraging existing home devices. 

## Main achievements:
  - Smart electrical heating management using existing basic heaters. Heating zones and quick entering of heating zone schedules 
  - Detailed energy monitoring. House thermal characteristics computation. Compute actual savings regarding heating schedules used
  - Legacy wired alarm appliance go to Internet
  - Cameras and motion detection with motionEye. End to end communication between motionEye and Domoticz 
  - Ligthing management reusing existing latching relays and wall pushbuttons
  - Robust implementation with two Domoticz synchronized servers (failover cluster) and alerting on sensors/actuators failure 

## Architecture:
  - Domoticz High Availability Cluster : Synology Dz V3.5877 (Main) - Raspberry Dz V4.97 (Backup) - Node.js scripts
  - Alarm server : Raspberry - motionEye - iot_ALARM-SVR Node.js script
  - Temperature Sensors and Lighting/Heaters Actuators : ESP8266 (Standalone ESP-12E and Electrodragon IoT ESP8266 Relay Board) - Arduino sketches
  - Communication protocol : MQTT

![Landscape Architecture](https://github.com/Antori91/Home_Automation/blob/master/Architecture%20Overview.GIF)

## Forum topics about:
  - https://www.domoticz.com/forum/viewtopic.php?f=38&t=17032
  
  - https://www.domoticz.com/forum/viewtopic.php?f=38&t=24088

  - https://www.domoticz.com/forum/viewtopic.php?f=38&t=21608

  - https://www.domoticz.com/forum/viewtopic.php?f=38&t=22436

  - https://www.domoticz.com/forum/viewtopic.php?f=38&t=23914

  - https://easydomoticz.com/forum/viewtopic.php?f=21&t=4798

## The cookbook:
  - For full configuration, setup three servers. Synology (or Raspberry#0): main Domoticz server, Raspberry#1: backup Domoticz server and Raspberry#2: dedicated Alarm server. Lite configuration can include for example only the main server (neither cluster feature nor alarm server). Mandatory software:
     - MQTT for the two Domoticz servers. On Raspbian Jessie:
     ```
         - wget http://repo.mosquitto.org/debian/mosquitto-repo.gpg.key
         - sudo apt-key add mosquitto-repo.gpg.key
         - cd /etc/apt/sources.list.d/
         - sudo wget http://repo.mosquitto.org/debian/mosquitto-jessie.list
         - sudo apt-get update
         - sudo apt-cache search mosquitto
         - sudo apt-get install mosquitto
     ```    
     - Node.js for all three (version greater than 6 for the two Raspberry#1 and #2) with packages mqtt (all), ssh2 (Raspberry#1), epoll and rpi-gpio (Raspberry#2):  
     ```
         - sudo apt-get update
         - sudo apt-get dist-upgrade
         - curl -sL https://deb.nodesource.com/setup_8.x | sudo -E bash -
         - sudo apt-get install -y nodejs
         - npm install mqtt
         - npm install ssh2
         - npm install epoll
         - npm install rpi-gpio
     ```    
     - nginx for Raspberry#1 (and Raspberry#0 if main server is not Synology). To install latest nginx (1.10.3) and ssl version on Raspbian Jessie:
     ```
         - sudo apt-get update
         - sudo apt-get dist-upgrade
         - sudo bash -c 'cat << EOF >> /etc/apt/sources.list.d/nginx.list
         -   deb http://httpredir.debian.org/debian/ jessie-backports main contrib non-free
         - EOF' 
         - gpg --keyserver keyserver.ubuntu.com --recv-key  8B48AD6246925553      
         - gpg -a --export 8B48AD6246925553 | sudo apt-key add -
         - gpg --keyserver keyserver.ubuntu.com --recv-key  7638D0442B90D010      
         - gpg -a --export 7638D0442B90D010 | sudo apt-key add -
         - sudo apt-get update     
         - sudo apt-get install -t jessie-backports nginx
         - sudo apt-get install -t jessie-backports openssl  
     ```    
  - Install motionEye in the dedicated Alarm server. On Raspbian Jessie:
     ```
     - sudo apt-get update
     - sudo apt-get install libav-tools
     - sudo apt-get install libpq5
     - sudo apt-get install libmysqlclient18
     - sudo wget https://github.com/Motion-Project/motion/releases/download/release-4.1.1/pi_jessie_motion_4.1.1-1_armhf.deb
     - sudo dpkg -i pi_jessie_motion_4.1.1-1_armhf.deb    
     - sudo apt-get install python-pip python-dev libssl-dev libcurl4-openssl-dev libjpeg-dev  
     - sudo pip install motioneye
     - sudo mkdir -p /etc/motioneye
     - sudo cp /usr/local/share/motioneye/extra/motioneye.conf.sample /etc/motioneye/motioneye.conf
     - sudo mkdir -p /var/lib/motioneye
     - sudo cp /usr/local/share/motioneye/extra/motioneye.systemd-unit-local /etc/systemd/system/motioneye.service
     - Update the /etc/motioneye/motion.conf file to have the following lines at the end of the file: webcontrol_html_output on, webcontrol_port XXXX where XXXX is the port number choosen (7999 by default), setup_mode off, webcontrol_parms 2 and webcontrol_localhost off
     - sudo systemctl daemon-reload
     - sudo systemctl enable motioneye
     - sudo systemctl start motioneye
     ```
  - Install Domoticz in the main server:
     - Hardware: 
     ```
         - "Dummy (Does nothing, use for virtual switches only)"
         - "MQTT Client Gateway with LAN interface" with Remote address=localhost, port=1883 and Publish Topic=out
     ```
     - Cameras: add all cameras
     - Devices (Except Security Panel, all are Dummy Virtual sensors): 
     ```
         - "Security Panel"
         - One protected "Light/Switch, Switch, Motion Sensor" for Alarm status and one protected "Light/Switch, Switch, On/Off" for Home Intrusion Alert. Define On/Off actions and Notifications to send email/sms (arm/disarm confirmation messages and intrusion alert message). To send SMS, I use Web REST API of the french FREE Telco operator 
         - One "Light/Switch, Switch, Media Player" for each (PTZ or non PTZ) camera and one additional "Light/Switch, Selector Switch, Selector" for each PTZ camera. For each "Light/Switch, Switch, Media Player" camera, define On/Off actions to activate/disactivate motionEye motion detection for this camera. For each "Light/Switch, Selector Switch, Selector", define the level names and actions according to the PTZ Camera setpoint commands
         - One "Temp+Hum" and one "Temperature" for each Temperature sensor and one "Temp+Hum" for Degrees.Day
         - One "Light/Switch, Switch, On/Off" for Electricity tariff
         - One "P1 Smart Meter, Energy" for each heater and one for overall heating index meter 
         - One "Light/Switch, Selector Switch, Selector" for Heating main breaker. Define the levels OFF/HORSGEL/ECO/CONFORT 
         - Two "Light/Switch, Selector Switch, Selector" for Heating Schedule/Start and Heating Schedule/Stop. Define the levels according to the heating zone names
         - One "General, Text" to display the heating zones status
         - One "Thermostat, SetPoint"
         - Two "General, Custom Sensor" for Thermal loss (°C/Degrees.Day unit) and Cooling rate (Wh/Degrees.Day unit)
         - One "Light/Switch, Switch, On/Off" for each lighting zone
         - Three protected "Light/Switch, Switch, Smoke Detector" for Failure status regarding Temperature sensors, Lighting server and Alarm server. Define On action and Notifications to send email/sms if failure 
     ```
     - User variables: 
     ```
         - Idx=1, "String" and name "HeatersActive" for Heaters status. Initial value: {"command" : "activateheaters", "28" : "On", "29" : "On", .... , "34" : "On"} where 28, 29 .... 34 are Heater IDX number
         - "Integer" and name "TWILIGHTimer0" for Lighting timer. Initial value: 0  
         - "Integer" and name "MezzaOverHeated" for one of the heating cost optimizer rules. Initial value: 0 
     ```         
     - Blockly: enter the blocklys according to the GIF images given
     - Scripts:
     ```     
         - Copy to the installation directory the nodejs and shell scripts. By default, this installation directory is "/volume1/@appstore/iot_domoticz" (Synology) and "/home/pi/iot_domoticz" (Raspberry)
         - Update the WiFi_DZ_MQTT_SecretKeys.js file according to the environment
         - Update myHeaters repository at line 133 of iot_Orchestrator.js file according to heating zones and heaters per heating zone used. Update also eventually lines 215 and 226 regarding Heating Schedule/Start and Heating Schedule/Stop actual names choosen
         - Update lines, with the [$$DOMOTICZ_PARAMETER] tag, of iot_ESP8266.js file according to the environment
         - Update Heaters repository and various parameters from line 74 to line 133 of iot_ACS712.js file 
         - Update also House thermal characteristics from line 40 to line 44 of iot_ACS712.js file  
         - Add tasks to Synology DSM task scheduler or Edit Crontab (Raspberry) to run all the shell scripts at boot
     ```    
     - Security setup: Local Networks (no username/password) set to accept connections without authentication from the Backup server and from "localhost;127.0.0.*"      
  - Backup the Domoticz database in the main server
  - Install Domoticz in the backup server:
     ```
     - Devices: import the main Domoticz database to setup again the devices
     - Blockly: importing at previous step the main Domoticz database also import the blocklys. Delete the HeatingOptimizer one
     - Scripts: copy again all the (modified) scripts and edit again Crontab
     ```
  -  Install the Cluster feature :
     ```
     - Copy the cluster scripts (nodejs and shell) to the backup server
     - Update myIDXtoSync repository at line 118 of mqtt_Cluster.js file according to the devices to synchronize 
     - Edit Crontab to run the shell script at boot
     - Create a DUMMY reverse proxy rule in DSM (use unreachable IP address/Port in your environment). Note the directory name created by DSM to store the ssl certificate. Update the main nginx configuration file with this ssl directory name 
     - Update again main server nginx config file and also backup server nginx config file to enter the correct path to Domoticz and motionEye
     - Copy the nginx configuration files to main and backup server
     - Test that your config files are ok with sudo nginx -t
     - Ask nginx to use your config files with sudo nginx -s reload
     ```
  - Install the Alarm scripts (nodejs and shell) in the dedicated Alarm server
  - Setup all the ESP8266:
     ``` 
     - Using Arduino IDE (Files/Preferences/Additional Board Manager set to http://arduino.esp8266.com/versions/2.3.0/package_esp8266com_index.json) 
         - Update WiFi_OTA_MQTT_SecretKeys.h according to the environment
         - Update PubSubClient.h file in the corresponding Arduino library directory
         - Update heaters repository from lines 130 to 166 of iot_ESP8266_AC712.ino file
         - Update lighting repository and various parameters from lines 36 to 74 of iot_ESP8266_GM43.ino file
         - Update temperature sensors repository and various parameters from lines 57 to 62 of iot_ESP8266_DHT22.ino file
         - Compile the sketches and flash the ESP8266  
     - Deploy all the ESP8266 connecting them to the home devices (heaters, hot water tank, lighting relays)
     ``` 
  - Time to use Domoticz...Enter in the main and backup Domoticz instances the heating zone schedules. To start or stop a heating zone at a given hour, you have to enter in Timers of Heating Schedule/Start or Heating Schedule/Stop devices the command ON on Time for the level corresponding to the heating zone. In the backup server, I've entered schedules to send every hour a start to all heating zones.           

           
