// @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
// V0.1 - July 2018 - Initial release

var mySecretKeys = {
MAIN_SERVER_IP          : '192.168.X.XXX',  // Main Domoticz server
BACKUP_SERVER_IP        : '192.168.X.XXX', // Backup Domoticz server
DZ_PORT                 :  XXXX,           // The Domoticz IP port         
idxClusterFailureFlag   : 'XX',            // The Domoticz Idx used to raise an alarm 
ProtectedDevicePassword : 'XXXX'           // The Domoticz password for protected device
};

module.exports   = mySecretKeys;