# Home Automation DIY project using ESP8266-Arduino, Node.js, Domoticz, Dahua Intercom and motionEye 
Powerful and low price sensors/actuators. Tailored solution leveraging existing home devices. 

## Main achievements:
  - Smart electrical heating management using existing basic heaters. Heating zones and quick entering of heating zone schedules 
  - Detailed energy monitoring with data from the French Linky energy meter (w/ Eesmart D2L ERL module) and each heater (w/ ACS712). House thermal characteristics computation. Compute actual savings regarding heating schedules used
  - Legacy wired alarm appliance go to Internet. Dahua VTH and Domoticz SecPanel sychronized together to arm/disarm the alarm
  - Cameras and motion detection with motionEye. End to end communication between motionEye and Domoticz 
  - Ligthing management reusing existing latching relays and wall pushbuttons
  - Versatile FireAlarm server using MQ-2 sensor
  - Robust implementation with two Domoticz synchronized servers (failover cluster) and alerting on sensors/actuators failure 

## Architecture:
  - Domoticz High Availability Cluster : Synology Dz V4.10693 (Main) - Raspberry Dz V4.10717 (Backup) - Node.js scripts
  - Alarm server : Raspberry - motionEye - iot_ALARM-SVR Node.js script
  - Temperature Sensors and Lighting/Heaters Actuators : ESP8266 (Standalone ESP-12E and Electrodragon IoT ESP8266 Relay Board) - Arduino sketches
  - Dahua Intercom using VTO and VTH modules
  - Communication protocol : MQTT

![Landscape Architecture](https://github.com/Antori91/Home_Automation/blob/master/Architecture%20Overview.GIF)
        

           
