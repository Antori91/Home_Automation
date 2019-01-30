#!/bin/sh
# @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
# ***** motionEye incoming connections tracking *****
# This script has to be run once after boot
# V0.2 - January 2019 - block incoming connections based on a IP address black list
# V0.1 - January 2019 - Initial version
sudo iptables -F
# Incoming connections Firewall rules - duplicate the following line to build the IP address black list
sudo iptables -A INPUT -p tcp --destination-port XXXX -m iprange --src-range YYY.0.0.0-YYYY.255.255.255 -j DROP
# Log the unblocked incoming connections 
sudo iptables -A INPUT -p tcp --dport XXXX --syn  -m limit --limit 4/minute -j LOG --log-prefix "MOTIONEYE: "
sudo iptables -L



