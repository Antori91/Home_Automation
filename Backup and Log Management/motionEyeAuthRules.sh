#!/bin/sh
# @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
# ***** motionEye incoming connections tracking *****
# V0.1 - January 2019 - Initial version
sudo iptables -I INPUT -p tcp --dport [YOUR_MOTIONEYE_PORT_NUMBER] --syn  -m limit --limit 4/minute -j LOG --log-prefix "MOTIONEYE: "
sudo iptables -L



