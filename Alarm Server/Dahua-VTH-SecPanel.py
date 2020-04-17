#!/usr/bin/env python3

"""
Author: Antori91 -- http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
DHIP/DVRIP routines -- https://github.com/mcw0/Tools/blob/master/Dahua-JSON-Debug-Console-v2.py
Subject: Dahua VTH used as Security Panel to arm/disarm an external non Dahua Alarm Appliance
V0.12 - April 2020 
      - Watchdog implementation
      - Allow keepAlive to fail for a few minutes before initiating a new VTH session
      - Fix: when VTH was disconnected, don't send anymore keepAlive message and discard MQTT messages       
V0.11 - April 2020 - Initial release 
V0.1g - March 2020 - BETA

=== Security concerns ===
MQTT: Alarm request Messages signed using MD5 hash.
VTH:  Service suspended and home automation alert sent if:
        - Fake or unknown VTH connected
        - Connection from any machine to VTH detected
        - Alarm request received having incorrect notifyConfigChange SID or request number
        - Alarm request received having notifyConfigChange values not matching actual VTH state 
      Alarm Disarm requests rejected if VTH connection is recent
"""

import sys, os
import json
import ndjson   # sudo pip3 install ndjson
import argparse
import copy 
from threading import Thread # High level python3 threading api 
import inspect
import datetime
import random
from OpenSSL import crypto # sudo pip3 install pyopenssl
from pwn import *   # https://github.com/Gallopsled/pwntools - sudo pip3 install pwntools. apt-get install libffi-dev libncurses5 libncurses5-dev libncursesw5
import paho.mqtt.client as mqtt # sudo pip3 install paho-mqtt

from VTOH_DZ_MQTT_SecretKeys import *

global debug
debug   = False
global verbose
verbose = False

SECURITY_BREACH      = 255
WATCHDOG_TRIGGER     = 600 # If P2P or MQTT fail for such time period in s, program terminates 
MAX_FAILED_KEEPALIVE = 300 # If VTH keepAlive fail for such time period in s, new VTH session is initiated
NORMAL_TERMINATION   = 0
WATCHDOG_TERMINATION = 1
DISARM_FREEZE_DELAY  = 60  # Following VTH login, disarming allowed only after a delay of n seconds  
SOCKET_TIMEOUT       = 5   # Main timeout in s used

# ---------------
# WATCHDOG
# ---------------

class _Watchdog:

    def __init__(self):
        self.WatchdogTrigger = WATCHDOG_TRIGGER
        
    def run(self, threadName):    
        if verbose: log.info("[" + str(datetime.datetime.now()) + " VTH_SecPanel-WATCHDOG] STARTED" )
        while self.WatchdogTrigger > 0:
            if not mqttc.connected_flag: time.sleep( 1 )
            mqttc.loop() # Process Mqtt stuff - 1 s blocking delay
            if( Dahua.VTH_ON_LINE != 0 or not mqttc.connected_flag): self.WatchdogTrigger -= 1
            else: self.WatchdogTrigger = WATCHDOG_TRIGGER


# ---------------
# MQTT FUNCTIONS
# ---------------

VTH_SecPanel_Hello = {
    "command" : "addlogmessage",
    "message" : "VTH SECPANEL ON LINE"
}

will_VTH_SecPanel = {
    "command" : "addlogmessage",
    "message" : "VTH SECPANEL went OFF LINE"
}

SECURITY_ALERT_VTH_SecPanel = {
    "command" : "addlogmessage",
    "message" : "VTH SECPANEL SECURITY BREACH - Service suspended"
}

WATCHDOG_ALERT_VTH_SecPanel = {
    "command" : "addlogmessage",
    "message" : "VTH SECPANEL WATCHDOG TRIGGER - Service respawning"
}
    
VTH_Hello = {
    "command" : "addlogmessage",
    "message" : "VTH BOX ON LINE"
}

will_VTH = {
    "command" : "addlogmessage",
    "message" : "VTH BOX went OFF LINE"
}    

dzGetSECPANEL = {
    "command" : "getdeviceinfo",
    "idx"     : mySecretKeys[ "DZ_idx_SecPanel" ]
}

dzAlarmRAISE_ALARM_FAILURE = {
    "command"   : "switchlight",
    "idx"       : mySecretKeys[ "idx_AlarmFailureFlag" ],
    "switchcmd" : "On",
    "passcode"  : mySecretKeys[ "ProtectedDevicePassword" ]
}


def GetDZSecPanel(threadName, delay):
    time.sleep( delay )
    log.info("[" + str(datetime.datetime.now()) + " VTH_SecPanel-MQTT_TX] {}".format(VTH_SecPanel_Hello))
    mqttc.publish(mySecretKeys[ "DZ_IN_TOPIC" ], json.dumps(VTH_SecPanel_Hello))
    if Dahua.VTH_ON_LINE == 0:
        log.info("[" + str(datetime.datetime.now()) + " VTH_SecPanel-MQTT_TX] {}".format(VTH_Hello))
        mqttc.publish(mySecretKeys[ "DZ_IN_TOPIC" ], json.dumps(VTH_Hello))    
    log.info("[" + str(datetime.datetime.now()) + " VTH_SecPanel-MQTT_TX] {}".format(dzGetSECPANEL))
    mqttc.publish(mySecretKeys[ "DZ_IN_TOPIC" ], json.dumps(dzGetSECPANEL))


def on_connect(mqttc, userdata, flags, rc):
    mqttc.connected_flag=True
    mqttc.subscribe(mySecretKeys[ "DZ_OUT_TOPIC" ])
    GetDZSecPanelThread=Thread( target=GetDZSecPanel, args=("GetDZSecPanel", 30,) )
    GetDZSecPanelThread.start()


def on_message(mqttc, userdata, msg):
    #if verbose: print( "MQTT message received, topic: " + msg.topic + ", msg: " + str(msg.payload) )
    JSONmsg = json.loads(msg.payload)
    if JSONmsg[ "idx" ] == mySecretKeys[ "idx_SecPanel" ]:
        log.info("[" + str(datetime.datetime.now()) + " VTH_SecPanel-MQTT_RX] {}".format(JSONmsg))
        if Dahua.VTH_ON_LINE != 0: log.info("[" + str(datetime.datetime.now()) + " VTH_SecPanel-MQTT_RX] MESSAGE IGNORED - VTH BOX is OFF LINE")
        else:
            Nonce = JSONmsg[ "description" ]
            tdiff = datetime.datetime.now() - datetime.datetime.strptime(Nonce[ "datetime" ], '%Y-%m-%d %H:%M:%S.%f')
            if ( tdiff.total_seconds() > 2 ) or ( JSONmsg['RSSI'] != hashlib.md5( (json.dumps(JSONmsg[ "description" ],separators=(',', ':')) + mySecretKeys[ "SecPanel_Seccode" ]).encode('latin-1') ).hexdigest() ): 
                log.warn("[" + str(datetime.datetime.now()) + " VTH_SecPanel-MQTT_RX] SECURITY WARNING - Message with invalid MD5 hash or datetime stamp received")
            elif ( AlarmToken[ "nvalue" ] != Nonce[ "nvalue" ] ):             
                if Dahua.VTH_SetSecPanel( Nonce[ "nvalue" ] ): # Change the VTH Alarm Enable/profile state           
                    log.info("[" + str(datetime.datetime.now()) + " VTH_SecPanel and VTH_BOX] FOLLOWING MQTT MESSAGE, SUCCESSFULL ALARM STATE MODIFICATION. VTH AlarmEnable is now: {}".format(Dahua.AlarmEnable)) 
                else: log.failure("[" + str(datetime.datetime.now()) + " VTH_SecPanel-P2P_FAILURE] SetSecPanel Failed") 

               
def on_disconnect(mqttc, userdata, rc):
    if rc != 0:
        log.failure("[" + str(datetime.datetime.now()) + " VTH-MQTT_FAILURE] MQTT went OFF LINE" )
        mqttc.connected_flag = False 
        MQTTerrorLogged      = True

        
# ---------------
# DAHUA FUNCTIONS
# ---------------

#
# DVRIP have different codes in their protocols
#
def DahuaProto(proto):

    proto = binascii.b2a_hex(proto.encode('latin-1')).decode('latin-1')

    headers = [
        'f6000000', # JSON Send
        'f6000068', # JSON Recv
        'a0050000', # DVRIP login Send Login Details
        'a0010060', # DVRIP Send Request Realm

        'b0000068', # DVRIP Recv
        'b0010068', # DVRIP Recv
    ]

    for code in headers:
        if code[:6] == proto[:6]:
            return True

    return False


def DEBUG(direction, packet):

    if debug:
        packet = packet.encode('latin-1')

        # Print send/recv data and current line number
        print("[BEGIN {}] <{:-^60}>".format(direction, inspect.currentframe().f_back.f_lineno))
        if (debug == 2) or (debug == 3):
            print(hexdump(packet))
        if (debug == 1) or (debug == 3):
            if packet[0:8] == p64(0x2000000044484950,endian='big') or DahuaProto(packet[0:4].decode('latin-1')):

                header = packet[0:32]
                data = packet[32:]

                if header[0:8] == p64(0x2000000044484950,endian='big'): # DHIP
                    print("\n-HEADER-  -DHIP-  SessionID   ID    RCVLEN             EXPLEN")
                elif DahuaProto(packet[0:4].decode('latin-1')): # DVRIP
                    print("\n PROTO   RCVLEN       ID            EXPLEN            SessionID")

                print("{}|{}|{}|{}|{}|{}|{}|{}".format(
                    binascii.b2a_hex(header[0:4]).decode('latin-1'),binascii.b2a_hex(header[4:8]).decode('latin-1'),
                    binascii.b2a_hex(header[8:12]).decode('latin-1'),binascii.b2a_hex(header[12:16]).decode('latin-1'),
                    binascii.b2a_hex(header[16:20]).decode('latin-1'),binascii.b2a_hex(header[20:24]).decode('latin-1'),
                    binascii.b2a_hex(header[24:28]).decode('latin-1'),binascii.b2a_hex(header[28:32]).decode('latin-1')))

                if data:
                    print("{}\n".format(data.decode('latin-1')))
            elif packet: # Unknown packet, do hexdump
                    log.failure("DEBUG: Unknow packet")
                    print(hexdump(packet))
        print("[ END  {}] <{:-^60}>".format(direction, inspect.currentframe().f_back.f_lineno))
    return

#
# From: https://github.com/haicen/DahuaHashCreator/blob/master/DahuaHash.py
#
#
def compressor(in_var, out):
    i=0
    j=0
    
    while i<len(in_var):
        # python 2.x (thanks to @davidak501)
        # out[j] = (ord(in_var[i]) + ord(in_var[i+1])) % 62;
        # python 3.x
        out[j] = (in_var[i] + in_var[i+1]) % 62;
        if (out[j] < 10):
            out[j] += 48
        elif (out[j] < 36):
          out[j] += 55;
        else:
            out[j] += 61        

        i=i+2
        j=j+1
        
def Dahua_Gen1_hash(passw):
#   if len(passw)>6:
#       debug("Warning: password is more than 6 characters. Hash may be incorrect")
    m = hashlib.md5()
    m.update(passw.encode("latin-1"))
    
    s=m.digest()
    crypt=[]
    for b in s:
        crypt.append(b)

    out2=['']*8
    compressor(crypt,out2)
    data=''.join([chr(a) for a in out2])
    return data
#
# END 
#

#
# Dahua DVRIP random MD5 password hash
#
def Dahua_DVRIP_md5_hash(Dahua_random, username, password):

    RANDOM_HASH = hashlib.md5((username + ':' + Dahua_random + ':' + Dahua_Gen1_hash(password)).encode('latin-1')).hexdigest().upper()

    return RANDOM_HASH

#
# Dahua random MD5 password hash
#
def Dahua_Gen2_md5_hash(Dahua_random, Dahua_realm, username, password):

    PWDDB_HASH = hashlib.md5((username + ':' + Dahua_realm + ':' + password).encode('latin-1')).hexdigest().upper()
    PASS = (username + ':' + Dahua_random + ':' + PWDDB_HASH).encode('latin-1')
    RANDOM_HASH = hashlib.md5(PASS).hexdigest().upper()

    return RANDOM_HASH


class Dahua_Functions:

    def __init__(self, rhost, rport, SSL, credentials, proto, force):
        self.rhost = rhost
        self.rport = rport
        self.SSL = SSL
        self.credentials = credentials
        self.proto = proto
        self.force = force

        # Internal sharing
        self.ID = 0                         # Our Request / Response ID that must be in all requests and initated by us
        self.SessionID  = 0                 # Session ID will be returned after successful login
        self.LoginTime  = '(null)'          # Login datetime 
        self.VTHCxion   = '(null)'          # Connection to VTH surveillance stamp         
        self.OBJECT = 0                     # Object ID will be returned after called <service>.factory.instance
        self.SID = 0                        # SID will be returned after we called <service>.attach with 'Object ID'
        self.SID_ID = 0                     # ID used when we called <service>.attach with 'Object ID'
        self.CALLBACK   = ''                # 'callback' ID will be returned after we called <service>.attach with 'proc: <Number>' (callback will have same number)
        self.FakeIPaddr = '(null)'          # WebGUI: mask our real IP
        self.clientType = ''                # WebGUI: We do not show up in logs or online users

        self.AlarmEnable   = False          # Remote VTH BOX Alarm status
        self.AlarmProfile  = ''             # Remote VTH BOX profile
        self.AlarmAlert    = False          # Remote VTH BOX alert output
        self.AlarmConfig   = ''             # Remote VTH BOX Alarm profile channels configuration
        self.VTH_ON_LINE   = -1             # Remote VTH BOX ON LINE status: -1 = Unknown, 0 = OK and >=1 (P2P error com line#)= KO
        self.MAX_KeepAlive_Failed = 0       # Max KeepAlive errors before VTH session reset
        self.KeepAlive_Failed = 0           # KeepAlive errors counter
        self.SERVICE_SUSPENDED = False      # If any security issue...
                                
        self.socket_event = threading.Event()
        self.lock = threading.Lock()
        
    #
    # This function will check and process any late incoming packets every second
    # At same time it will act as the delay for keepAlive of the connection
    #
    def sleep_check_socket(self,delay):
        keepAlive = 0
        sleep = 1

        while True:
            if delay <= keepAlive:
                break
            else:
                keepAlive += sleep
                # If received callback data, break
                if self.remote.can_recv():
                    break
                time.sleep(sleep)
                continue


    def P2P_timeout(self,threadName,delay,stop):

        if verbose: log.success("Started keepAlive thread")

        while True:
            self.sleep_check_socket(delay)
            query_args = {
                "method":"global.keepAlive",
                "magic" : "0x1234",
                "params":{
                    "timeout":delay,
                    "active":True
                    },
                "id":self.ID,
                "session":self.SessionID}
            
            if stop(): break              # stop the thread received
            if self.VTH_ON_LINE != 0: continue # VTH is disconnected, don't send keepAlive message
      
            data = self.P2P(json.dumps(query_args))
            if data == None:  # keepAlive failed
                self.KeepAlive_Failed -= 1
                if self.KeepAlive_Failed <= 0: self.VTH_ON_LINE = self.P2P_traceError() # KO 
            elif len(data) == 1:            
                if verbose: log.info("P2P-1.Callback message received: {}".format(data))
                data = json.loads(data)
                if data.get('result'):
                    self.VTH_ON_LINE = 0 # OK
                    self.KeepAlive_Failed = self.MAX_KeepAlive_Failed # keepAlive back        
                else:  # keepAlive failed
                    self.KeepAlive_Failed -= 1
                    if self.KeepAlive_Failed <= 0: self.VTH_ON_LINE = self.P2P_traceError() # KO 
            else:
                if verbose: log.info("P2P-2.Callback message received: {}".format(data))
                data = ndjson.loads(data)
                
                if data[0].get('method') == "client.notifyConfigChange":
                    if self.VTHCxion != hashlib.md5(open('/home/pi/iot_domoticz/Dahua-VTH-BOX-cxion.log','rb').read()).hexdigest():
                        log.failure( "[" + str(datetime.datetime.now()) + " VTH_BOX-Notification] SECURITY WARNING - Unknown connection to VTH happened. SERVICE SUSPENDED." )
                        self.SERVICE_SUSPENDED = True           
                    elif data[0]['params']['SID'] != self.SID:
                        log.failure( "[" + str(datetime.datetime.now()) + " VTH_BOX-Notification] SECURITY WARNING - Session ID incorrect. SERVICE SUSPENDED." )
                        self.SERVICE_SUSPENDED = True
                    elif data[0]['id'] != self.SID_ID:
                        log.failure( "[" + str(datetime.datetime.now()) + " VTH_BOX-Notification] SECURITY WARNING - Request ID incorrect. SERVICE SUSPENDED." )
                        self.SERVICE_SUSPENDED = True
                    else:
                        tdiff = datetime.datetime.now() - self.LoginTime
                        if ( (tdiff.total_seconds() / DISARM_FREEZE_DELAY) < 1 ) and not data[0]['params']['table']['AlarmEnable']:
                            log.warn("[" + str(datetime.datetime.now()) + " VTH_BOX-Notification] SECURITY WARNING - Recent Login to VTH - IGNORING DISARM request")
                            if self.VTH_SetSecPanel( AlarmToken['nvalue'] ): log.info("[" + str(datetime.datetime.now()) + " VTH_BOX-Alarm] been reset to previous state") 
                            else: 
                                log.failure("[" + str(datetime.datetime.now()) + " VTH_SecPanel-P2P_FAILURE] SetSecPanel Failed. SERVICE SUSPENDED.")
                                self.SERVICE_SUSPENDED = True             
                        else:  
                            _AlarmEnable  = data[0]['params']['table']['AlarmEnable']
                            _AlarmProfile = data[0]['params']['table']['CurrentProfile']
                            _AlarmConfig  = data[0]['params']['table']['Profiles']
                            if self.VTH_GetSecPanel(): 
                                if _AlarmEnable == self.AlarmEnable and _AlarmProfile == self.AlarmProfile and _AlarmConfig == self.AlarmConfig:
                                    log.info("[" + str(datetime.datetime.now()) + " VTH_SecPanel and VTH_BOX] FOLLOWING VTH USER OPERATION, SUCCESSFULL ALARM STATE MODIFICATION. VTH AlarmEnable is now: {}".format(self.AlarmEnable))  
                                    Nonce = {  
                                    "stationID" : mySecretKeys[ "VTH_ALARM_CID" ],
                                    "datetime"  : str(datetime.datetime.now()), 
                                    "nvalue"    : AlarmToken['nvalue']
                                    }
                                    AlarmToken['description'] = Nonce
                                    AlarmToken['RSSI']        = hashlib.md5( (json.dumps(AlarmToken['description'],separators=(',', ':')) + mySecretKeys[ "SecPanel_Seccode" ]).encode('latin-1') ).hexdigest()
                                    self.VTH_ON_LINE = 0 # OK
                                    log.info("[" + str(datetime.datetime.now()) + " VTH_SecPanel-MQTT_TX] {}".format(AlarmToken))
                                    mqttc.publish(mySecretKeys[ "DZ_OUT_TOPIC" ], json.dumps(AlarmToken)) #Inform Alarm server and other Alarm clients
                                else:   
                                    log.failure( "[" + str(datetime.datetime.now()) + " VTH_BOX-Notification] SECURITY WARNING - Notification doesn't match VTH state. SERVICE SUSPENDED." )
                                    self.SERVICE_SUSPENDED = True    
                            else:
                                log.failure("[" + str(datetime.datetime.now()) + " VTH_SecPanel-P2P_FAILURE] GetSecPanel Failed. SERVICE SUSPENDED.")
                                self.SERVICE_SUSPENDED = True   
                
                for NUM in range(0,len(data)):
                    if data[NUM].get('result'): 
                        self.VTH_ON_LINE = 0 # OK  
                        self.KeepAlive_Failed = self.MAX_KeepAlive_Failed  # keepAlive Back                           
                    else:  # keepAlive failed
                        self.KeepAlive_Failed -= 1
                        if self.KeepAlive_Failed <= 0: self.VTH_ON_LINE = self.P2P_traceError() # KO 

   
    def P2P_traceError(self):
        cf = inspect.currentframe()
        return cf.f_back.f_lineno    

                    
    def P2P(self, packet):
        P2P_header = ""
        P2P_data = ""
        P2P_return_data = []
        header_LEN = 0
        LEN_RECVED = 0
        data = ''

        if packet == None:
            packet = ''

        header = copy.copy(self.header)
        header = header.replace('_SessionHexID_'.encode('latin-1'),p32(self.SessionID))
        header = header.replace('_LEN_'.encode('latin-1'),p32(len(packet)))
        header = header.replace('_ID_'.encode('latin-1'),p32(self.ID))

        try:
            if not len(header) == 32:
                log.error("Binary header != 32 ({})".format(len(header)))
        except Exception as e:
            return None

        self.ID += 1
        
        self.lock.acquire()
        
        DEBUG("SEND",header.decode('latin-1') + packet)

        try:
            self.remote.send(header + packet.encode('latin-1'))
        except Exception as e:
            if self.lock.locked():
                self.lock.release()
            self.socket_event.set()
            log.failure("[" + str(datetime.datetime.now()) + " VTH-P2P_FAILURE] Sending failure with error code: {}".format(str(e)) )
            self.VTH_ON_LINE = self.P2P_traceError() # KO

        #
        # We must expect there is no output from remote device
        # Some debug cmd do not return any output, some will return after timeout/failure, most will return directly
        #
        TIME = 0.5
        TIMEOUT = TIME

        while True:
            try:
                tmp = len(data)
                data += self.remote.recv(numb=8192,timeout=TIMEOUT).decode('latin-1')
                if not len(data):
                    # Give few more seconds if there are not data, P2P_timeout() will take anything later
                    if TIMEOUT == TIME:
                        TIMEOUT = 2
                        continue
                if tmp == len(data):
                    break
            except Exception as e:
                self.socket_event.set()
                return None

        if not len(data):
            if self.lock.locked():
                self.lock.release()
                log.warn("[" + str(datetime.datetime.now()) + " VTH-P2P_FAILURE] No answer from VTH. Packet sent to VTH BOX was: " + packet )
                return None

        while len(data):
            # DHIP
            if data[0:8] == p64(0x2000000044484950,endian='big').decode('latin-1'):
                P2P_header = data[0:32]
                LEN_RECVED = unpack(data[16:20].encode('latin-1'))
                LEN_EXPECT = unpack(data[24:28].encode('latin-1'))
                data = data[32:]
            # DVRIP
            elif DahuaProto(data[0:4]):
                LEN_RECVED = unpack(data[4:8].encode('latin-1'))
                LEN_EXPECT = unpack(data[16:20].encode('latin-1'))
                P2P_header = data[0:32]

                if P2P_header[24:28].encode('latin-1') == p32(0x0600f900,endian='big'):
                    self.SessionID = unpack(P2P_header[16:20].encode('latin-1'))
                    self.AuthCode = binascii.b2a_hex(P2P_header[28:32].encode('latin-1')).decode('latin-1')
                    self.ErrorCode = binascii.b2a_hex(P2P_header[8:12].encode('latin-1')).decode('latin-1')
                if len(data) == 32:
                    DEBUG("RECV",P2P_header)
                    if self.lock.locked():
                        self.lock.release()
                data = data[32:]
            else:
                if LEN_RECVED == 0:
                    log.failure("[" + str(datetime.datetime.now()) + " VTH-P2P_FAILURE] Unknow packet:" )
                    print("PROTO: \033[92m[\033[91m{}\033[92m]\033[0m".format(binascii.b2a_hex(data[0:4].encode('latin-1')).decode('latin-1')))
                    print(hexdump(data))
                    self.VTH_ON_LINE = self.P2P_traceError() # KO
                    return None
                P2P_data = data[0:LEN_RECVED]
                if LEN_RECVED:
                    DEBUG("RECV",P2P_header + data[0:LEN_RECVED])
                else:
                    DEBUG("RECV",P2P_header)
                P2P_return_data.append(P2P_data)
                data = data[LEN_RECVED:]
                if self.lock.locked():
                    self.lock.release()
                if LEN_RECVED == LEN_EXPECT and not len(data):
                    break

        if verbose: log.info("[" + str(datetime.datetime.now()) + " VTH-P2P] ACK. Packet sent to VTH BOX was: " + packet )
        return ''.join(map(str, P2P_return_data))

    
    def Dahua_Login(self):

        if self.lock.locked(): self.lock.release()
        self.ID = random.randint(0,9999)
        context.log_level = 'CRITICAL'            
        try:
            self.remote = remote(self.rhost, self.rport, ssl=self.SSL, timeout=SOCKET_TIMEOUT)
            context.log_level = 'INFO' 
        except (Exception, KeyboardInterrupt, SystemExit):
            context.log_level = 'INFO'
            self.VTH_ON_LINE = self.P2P_traceError() # KO
            return False
          
        if self.proto == 'dvrip':
            if not self.Dahua_DVRIP_Login():
                self.VTH_ON_LINE = self.P2P_traceError() # KO
                log.failure("Failed to login")
                return False
        elif self.proto == 'dhip':
            if not self.Dahua_DHIP_Login():
                self.VTH_ON_LINE = self.P2P_traceError() # KO
                log.failure("Failed to login")
                return False
        else:
            log.failure("Failed to login - Protocol not set to dhip or dvrip")
            self.VTH_ON_LINE = -1
            self.SERVICE_SUSPENDED = True
            return False
        
        self.LoginTime   = datetime.datetime.now()
        self.VTHCxion    = hashlib.md5(open('/home/pi/iot_domoticz/Dahua-VTH-BOX-cxion.log','rb').read()).hexdigest()
        if verbose: log.info("[" + str(datetime.datetime.now()) + "  self.ID]  initialized to {}".format( self.ID))
        self.VTH_ON_LINE = 0 # OK
        return True   
            
    def Dahua_DHIP_Login(self):

        if verbose: login = log.progress("Login")

        self.header =  p64(0x2000000044484950,endian='big') +'_SessionHexID__ID__LEN_'.encode('latin-1') + p32(0x0) +'_LEN_'.encode('latin-1')+ p32(0x0) 

        USER_NAME = self.credentials.split(':')[0]
        PASSWORD = self.credentials.split(':')[1]

        query_args = {
            "id" : 10000,
            "magic":"0x1234",
            "method":"global.login",
            "params":{
                "clientType":self.clientType,
                "ipAddr":self.FakeIPaddr,
                "loginType":"Direct",
                "password":"",
                "userName":USER_NAME,
                },
            "session":0
            }

        data = self.P2P(json.dumps(query_args))
        if data == None:
            if verbose: login.failure("global.login [random]")
            return False
        data = json.loads(data)

        self.SessionID = data['session']
        RANDOM = data['params']['random']
        REALM = data['params']['realm']

        RANDOM_HASH = Dahua_Gen2_md5_hash(RANDOM, REALM, USER_NAME, PASSWORD)

        query_args = {
            "id":10000,
            "magic":"0x1234",
            "method":"global.login",
            "session":self.SessionID,
            "params":{
                "userName":USER_NAME,
                "password":RANDOM_HASH,
                "clientType":self.clientType,
                "ipAddr" : self.FakeIPaddr, 
                "loginType" : "Direct",
                "authorityType":"Default",
                },
            }

        data = self.P2P(json.dumps(query_args))
        if data == None:
            return False
        data = json.loads(data)

        if not data.get('result'):
            if verbose: login.failure("global.login: {}".format(data['error']['message'])) 
            return False

        keepAlive = data['params']['keepAliveInterval']
        self.MAX_KeepAlive_Failed = int( MAX_FAILED_KEEPALIVE / keepAlive )
        self.KeepAlive_Failed     = self.MAX_KeepAlive_Failed
        if verbose: log.info("VTH KEEPALIVE: {}".format(keepAlive))
        if coldBootP2P: 
            global P2P_timeout_Thread
            P2P_timeout_Thread = Thread( target=self.P2P_timeout, args=("P2P_timeout", keepAlive, lambda: stop_threads,) )
            P2P_timeout_Thread.start()
        
        if verbose: login.success("Success")

        return True


    def Dahua_DVRIP_Login(self):

        if verbose: login = log.progress("Login")

        USER_NAME = self.credentials.split(':')[0]
        PASSWORD = self.credentials.split(':')[1]

        #
        # REALM & RANDOM Request
        #
        self.header = p32(0xa0010000,endian='big') + (p8(0x00) * 20) + p64(0x050201010000a1aa,endian='big')

        data = self.P2P(None)
        if data == None or not len(data):
            if verbose: login.failure("Realm")
            return False

        REALM = data.split('\r\n')[0].split(':')[1] if data.split('\r\n')[0].split(':')[0] == 'Realm' else False
        RANDOM = data.split('\r\n')[1].split(':')[1] if data.split('\r\n')[1].split(':')[0] == 'Random' else False

        if not RANDOM:
            if verbose: login.failure("Realm [random]")
            return False

        #
        # Login request
        #

        HASH = USER_NAME + '&&' + Dahua_Gen2_md5_hash(RANDOM, REALM, USER_NAME, PASSWORD) + Dahua_DVRIP_md5_hash(RANDOM, USER_NAME, PASSWORD)
        self.header = p32(0xa0050000,endian='big') + p32(len(HASH)) + (p8(0x00) * 16) + p64(0x050200080000a1aa,endian='big')

        data = self.P2P(HASH)
        if data == None:
            return False
#       if len(data):
#           print(data)


        if self.ErrorCode[:4] == '0008':
            if verbose: login.success("Success")
        elif self.ErrorCode[:4] == '0100':
            if verbose: login.failure("Authentication failed: {} tries left {}".format(int(self.AuthCode[2:4],16), "(BUG: SessionID = {})".format(self.SessionID) if self.SessionID else ''))
            return False
        elif self.ErrorCode[:4] == '0101':
            if verbose: login.failure("Username invalid")
            return False
        elif self.ErrorCode[:4] == '0104':
            if verbose: login.failure("Account locked: {}".format(data))
            return False
        elif self.ErrorCode[:4] == '0105':
            if verbose: login.failure("Undefined code: {}".format(self.ErrorCode[:4]))
            return False
        elif self.ErrorCode[:4] == '0113':
            if verbose: login.failure("Not implemented: {}".format(self.ErrorCode[:4]))
            return False
        elif self.ErrorCode[:4] == '0303':
            if verbose: login.failure("User already connected")
            return False
        else:
            if verbose: login.failure("Unknown ErrorCode: {}".format(self.ErrorCode[:4]))
            return False

        keepAlive = 30 # Seems to be stable
        self.MAX_KeepAlive_Failed = int( MAX_FAILED_KEEPALIVE / keepAlive )
        self.KeepAlive_Failed     = self.MAX_KeepAlive_Failed
        if verbose: log.info("VTH KEEPALIVE: {}".format(keepAlive))
        if coldBootP2P: 
            global P2P_timeout_Thread
            P2P_timeout_Thread = Thread( target=self.P2P_timeout, args=("P2P_timeout", keepAlive, lambda: stop_threads,) )
            P2P_timeout_Thread.start()
            
        self.header =  p32(0xf6000000,endian='big') + '_LEN__ID_'.encode('latin-1') + p32(0x0) + '_LEN_'.encode('latin-1') + p32(0x0) + '_SessionHexID_'.encode('latin-1') + p32(0x0)

        return True

    
    def VTH_GetSecPanel(self):
           
        query_args = {
                "method":"configManager.getConfig",
                "magic" : "0x1234",
                "params":{
                    "name": "CommGlobal",
                    },
                "session":self.SessionID,
                "id":self.ID
        }
        
        data = self.P2P(json.dumps(query_args))
        if data == None:
            log.failure("[" + str(datetime.datetime.now()) + " VTH-P2P_FAILURE] GetSecPanel Failed" )
            self.VTH_ON_LINE = self.P2P_traceError() # KO  
            return False
        
        if verbose: log.info("VTH_GetSecPanel Service Call answer: {}".format(data))
        data = json.loads(data)
        self.AlarmEnable  = data['params']['table']['AlarmEnable']
        self.AlarmProfile = data['params']['table']['CurrentProfile']
        self.AlarmConfig  = data['params']['table']['Profiles']
        
        if not self.AlarmEnable: AlarmToken[ "nvalue" ] = 0
        else: AlarmToken[ "nvalue" ] = VTHAlarmProfile[ self.AlarmProfile ]
        if verbose: log.info("[" + str(datetime.datetime.now()) + " VTH_BOX-AlarmEnable] is: {}".format(self.AlarmEnable)) 
        if verbose: log.info("[" + str(datetime.datetime.now()) + " VTH_BOX-AlarmProfile] is: {}".format(self.AlarmProfile))
        if verbose: log.info("[" + str(datetime.datetime.now()) + " VTH_BOX-AlarmConfiguration] is: {}".format(self.AlarmConfig))
        log.info("[" + str(datetime.datetime.now()) + " VTH_SecPanel-AlarmToken] updated from VTH BOX to: {}".format(AlarmToken))
        self.VTH_ON_LINE = 0 # OK
        return True
 

    def VTH_GetSecPanelChange(self):

        query_args = {
                "method":"configManager.attach",
                "magic" : "0x1234",
                "params":{
                    "name": "CommGlobal",
                    },
                "session":self.SessionID,
                "id":self.ID
        }
        
        data = self.P2P(json.dumps(query_args))
        if data == None:
            log.failure("[" + str(datetime.datetime.now()) + " VTH-P2P_FAILURE] GetSecPanelChange Failed" )
            self.VTH_ON_LINE = self.P2P_traceError() # KO 
            return False

        if verbose: log.info("VTH_GetSecPanelChange Service Call answer: {}".format(data))   
        data = json.loads(data)       
        self.SID    = data['params']['SID']
        self.SID_ID = data['id']

        self.VTH_ON_LINE = 0 # OK
        return True


    def VTH_SetSecPanel(self, CVQ6081_Alarm):    

        if CVQ6081_Alarm == 0:      
            self.AlarmEnable = False
        else: 
            self.AlarmEnable = True            
            self.AlarmProfile = AlarmProfile[ CVQ6081_Alarm ]   

        query_args = {
                "method":"configManager.setConfig",
                "magic" : "0x1234",
                "params":{
                    "table":{
                        "AlarmEnable"    : self.AlarmEnable,
                        "CurrentProfile" : self.AlarmProfile,
                        "ProfileEnable"  : True,
                        "Profiles"       : self.AlarmConfig
                    },
                    "name":"CommGlobal",
                },
                "session":self.SessionID,
                "id":self.ID
        }
                
        if verbose: log.info("[" + str(datetime.datetime.now()) + " VTH_BOX] Updating to: {}".format(query_args))    
        if verbose: log.info("VTH_SetSecPanel Service Call request: {}".format(json.dumps(query_args)))
        
        data = self.P2P(json.dumps(query_args))
        
        if data == None:
            log.failure("[" + str(datetime.datetime.now()) + " VTH_BOX-P2P_FAILURE] SetSecPanel Failed - No answer" )
            self.VTH_ON_LINE = self.P2P_traceError() # KO 
            return False
        elif len(data) == 1:            
            if verbose: log.info("P2P-1. VTH_SetSecPanel Service Call answer: {}".format(data))
            data = json.loads(data)
            if data.get('result'):
                self.VTH_ON_LINE = 0 # OK
            else: 
                self.VTH_ON_LINE = self.P2P_traceError() # KO
                return False
        else:
            if verbose: log.info("P2P-2. VTH_SetSecPanel Service Call answer: {}".format(data))
            data = ndjson.loads(data)               
            if data[0].get('method') == "client.notifyConfigChange":
                self.AlarmEnable  = data[0]['params']['table']['AlarmEnable']
                self.AlarmProfile = data[0]['params']['table']['CurrentProfile']
                self.AlarmConfig  = data[0]['params']['table']['Profiles']
                if verbose: log.info("[" + str(datetime.datetime.now()) + " VTH_BOX-AlarmEnable] been changed remotely to: {}".format(self.AlarmEnable)) 
                if verbose: log.info("[" + str(datetime.datetime.now()) + " VTH_BOX-AlarmProfile] is: {}".format(self.AlarmProfile))
                if verbose: log.info("[" + str(datetime.datetime.now()) + " VTH_BOX-AlarmConfiguration] is: {}".format(self.AlarmConfig))
                if not self.AlarmEnable:
                    AlarmToken['nvalue'] = 0
                else: 
                    AlarmToken['nvalue'] = VTHAlarmProfile[ self.AlarmProfile ]              
                
        self.VTH_ON_LINE = 0 # OK
        return True


    def VTH_CheckSerialNumber(self):
        
        query_args = {
            "method":"magicBox.getSerialNo",
            "magic" : "0x1234",
            "params":"null",
            "session":self.SessionID,
            "id":self.ID
            }
        data = self.P2P(json.dumps(query_args))
        if not data == None:
            data = json.loads(data)
            if data.get('result'):
                if verbose: log.info("[" + str(datetime.datetime.now()) + " VTH_BOX-SerialNumber] {}".format(data))
                if mySecretKeys[ "VTH_SERIAL_NUMBER" ] != data['params']['sn']:
                    self.VTH_ON_LINE = self.P2P_traceError() # KO
                    log.failure( "[" + str(datetime.datetime.now()) + " VTH_SERIAL_NUMBER] SECURITY WARNING - WRONG Serial Number.  SERVICE SUSPENDED." )
                    self.SERVICE_SUSPENDED = True   
                    return False
                self.VTH_ON_LINE = 0 # OK
                return True
            else:
                log.failure("[" + str(datetime.datetime.now()) + " VTH_BOX-SerialNumber-P2P_FAILURE] CheckSerialNumber Failed - Error code: {}".format(data)) 
                self.VTH_ON_LINE = self.P2P_traceError() # KO
                log.failure( "[" + str(datetime.datetime.now()) + " VTH_SERIAL_NUMBER] SECURITY WARNING - Serial Number UNKNOWN.  SERVICE SUSPENDED." )
                self.SERVICE_SUSPENDED = True   
                return False
        else:
            log.failure("[" + str(datetime.datetime.now()) + " VTH_BOX-SerialNumber-P2P_FAILURE] CheckSerialNumber Failed - No answer" )
            self.VTH_ON_LINE = self.P2P_traceError() # KO  
            log.failure( "[" + str(datetime.datetime.now()) + " VTH_SERIAL_NUMBER] SECURITY WARNING - Serial Number UNKNOWN.  SERVICE SUSPENDED." )
            self.SERVICE_SUSPENDED = True   
            return False        

                
    def logout(self):
        
        if self.VTHCxion != hashlib.md5(open('/home/pi/iot_domoticz/Dahua-VTH-BOX-cxion.log','rb').read()).hexdigest():
            log.failure( "[" + str(datetime.datetime.now()) + " VTH_BOX-Notification] SECURITY WARNING - Unknown connection to VTH happened. SERVICE SUSPENDED." )
            self.SERVICE_SUSPENDED = True
        
        query_args = {
            "method":"global.logout",
            "params":"null",
            "session":self.SessionID,
            "id":self.ID
            }
        data = self.P2P(json.dumps(query_args))
        if not data == None:
            data = json.loads(data)
            if data.get('result'):
                if verbose: log.info("[" + str(datetime.datetime.now()) + " VTH_SecPanel-P2P_Info] Global.logout OK" )
                self.remote.close()
                return True
            else:
                log.failure("[" + str(datetime.datetime.now()) + " VTH_SecPanel-P2P_FAILURE] Global.logout error code: {}".format(data)) 
                self.remote.close()
                return False
        else:
            log.warn("[" + str(datetime.datetime.now()) + " VTH_SecPanel-P2P_FAILURE] Global.logout without ACK" )
            self.remote.close()
            return False
        
        
# ---------------
# MAIN
# ---------------

if __name__ == '__main__':


    INFO = "[" + str(datetime.datetime.now()) + " iot_Dahua_VTH_SecPanel V0.12 starting]"
    
    AlarmProfile    = { 0: "AlarmDisable", 9: "Outdoor", 11: "AtHome" }             # DZ SecPanel to corresponding VTH AlarmEnable/profile
    VTHAlarmProfile = { "Outdoor": 9, "AtHome": 11, "Sleeping": 11, "Custom": 11 }  # VTH to corresponding DZ SecPanel for VTH AlarmEnable=True       
    AlarmToken = {  
    "Battery" : 255,
    "RSSI" : '',                                     # MD5 message digest
    "description" : mySecretKeys[ "VTH_ALARM_CID" ], # nonce = stationID + datetime + nvalue
    "dtype" : "Security",
    "id" : "148702",
    "idx" : mySecretKeys[ "idx_SecPanel" ],          # SecPanel idx
    "name" : "Security Panel",
    "nvalue" : 1,                                    # Alarm Enable/profile
    "stype" : "Security Panel",
    "switchType" : "On/Off",
    "unit" : 0
    }  
        
    log.info( INFO )
    Rssl = False  # SSL used to communicate with the VTH
    log.info("VTH HOST: {}".format(mySecretKeys[ "VTH_SERVER_IP" ]))
    log.info("VTH PORT: {}".format(mySecretKeys[ "VTH_PORT" ]))
    log.info("VTH PROTOCOL: {}".format(mySecretKeys[ "VTH_PROTOCOL" ]))
    log.info("VTH SSL: {}".format(Rssl))  
    
    # Start mqtt and subscribe to Alarm topic  
    coldBootMQTT         = True
    mqttc                = mqtt.Client()
    mqttc.will_set(mySecretKeys[ "DZ_IN_TOPIC" ], json.dumps(will_VTH_SecPanel))
    MQTTerrorLogged      = False  # MQTT issue catched and already logged
    mqttc.on_connect     = on_connect
    mqttc.on_message     = on_message
    mqttc.on_disconnect  = on_disconnect
    mqttc.connected_flag = False      
        
    # Start Dahua functions  
    coldBootP2P    = True
    P2PerrorLogged = False  # P2P issue catched and already logged      
    Dahua = Dahua_Functions(mySecretKeys[ "VTH_SERVER_IP" ], mySecretKeys[ "VTH_PORT" ], Rssl, mySecretKeys[ "VTH_CREDENTIALS" ], mySecretKeys[ "VTH_PROTOCOL" ], False)   
    
    # Start Watchdog
    stop_threads   = False
    Watchdog       = _Watchdog()
    WatchdogThread = Thread( target=Watchdog.run, args=("WATCHDOG", ) )
    WatchdogThread.start()

    while True:                
        if not mqttc.connected_flag: 
            try:
                if coldBootMQTT:
                    mqttc.connect(mySecretKeys[ "MAIN_SERVER_IP" ], mySecretKeys[ "MQTT_PORT" ]) 
                    log.success( "[" + str(datetime.datetime.now()) + " VTH-MQTT_OK] CONNECTED to MQTT" )
                    coldBootMQTT = False
                else: 
                    mqttc.reconnect()
                    log.success( "[" + str(datetime.datetime.now()) + " VTH-MQTT_OK] MQTT back ON LINE" )
                    time.sleep(0.3)
                MQTTerrorLogged = False
            except Exception as e:
                if coldBootMQTT and not MQTTerrorLogged:
                    log.failure( "[" + str(datetime.datetime.now()) + " VTH-MQTT_FAILURE] MQTT is DOWN - Trying to connect every " + str(SOCKET_TIMEOUT) + "s" )
                    MQTTerrorLogged = True
                    time.sleep(SOCKET_TIMEOUT)
                             
        if Dahua.VTH_ON_LINE != 0:  # We are not connected or we lost the VTH Box
            if coldBootP2P:
                if Dahua.Dahua_Login():                 
                    if not Dahua.VTH_CheckSerialNumber(): break
                    log.success( "[" + str(datetime.datetime.now()) + " VTH-P2P_OK] CONNECTED to VTH BOX" )
                    if Dahua.VTH_GetSecPanel():   
                        if Dahua.VTH_GetSecPanelChange():
                            P2PerrorLogged = False
                            coldBootP2P    = False
                else:         
                    if not P2PerrorLogged: log.failure( "[" + str(datetime.datetime.now()) + " VTH-P2P_FAILURE] VTH BOX is DOWN - Trying to connect every " + str(SOCKET_TIMEOUT) + "s" )
                    P2PerrorLogged = True
            else:
                if not P2PerrorLogged:
                    log.failure( "[" + str(datetime.datetime.now()) + " VTH-P2P_FAILURE] VTH BOX went OFF LINE - P2P error at line#: " + str(Dahua.VTH_ON_LINE) )
                    mqttc.publish(mySecretKeys[ "DZ_IN_TOPIC" ], json.dumps(will_VTH))  
                    time.sleep(0.3)  
                    try:                        
                        Dahua.logout()
                        time.sleep(0.3)
                    except Exception as e:
                        time.sleep(0.3)
                    P2PerrorLogged = True
                if Dahua.Dahua_Login():  
                    if not Dahua.VTH_CheckSerialNumber(): break 
                    log.success( "[" + str(datetime.datetime.now()) + " VTH-P2P_OK] VTH BOX back ON LINE" )
                    log.info("[" + str(datetime.datetime.now()) + " VTH_SecPanel-MQTT_TX] {}".format(VTH_Hello))
                    mqttc.publish(mySecretKeys[ "DZ_IN_TOPIC" ], json.dumps(VTH_Hello))
                    log.info("[" + str(datetime.datetime.now()) + " VTH_SecPanel-MQTT_TX] {}".format(dzGetSECPANEL))
                    mqttc.publish(mySecretKeys[ "DZ_IN_TOPIC" ], json.dumps(dzGetSECPANEL))
                    if Dahua.VTH_GetSecPanel():   
                        if Dahua.VTH_GetSecPanelChange(): P2PerrorLogged  = False
                  
        time.sleep( 0.5 )          
        if Dahua.SERVICE_SUSPENDED: break
        if Watchdog.WatchdogTrigger <= 0: break
   
    # We should never arrive here...unless Security issue or Watchdog trigger
    if Watchdog.WatchdogTrigger <= 0: WatchdogFlag=True
    else: WatchdogFlag=False
    log.failure("[" + str(datetime.datetime.now()) + " VTH_SecPanel-Disconnecting] SERVICE_SUSPENDED: {}".format(Dahua.SERVICE_SUSPENDED) + " - WATCHDOG: {}".format(WatchdogFlag))   
    if Dahua.SERVICE_SUSPENDED and mqttc.connected_flag: 
        mqttc.publish(mySecretKeys[ "DZ_IN_TOPIC" ], json.dumps(dzAlarmRAISE_ALARM_FAILURE))        
        mqttc.publish(mySecretKeys[ "DZ_IN_TOPIC" ], json.dumps(SECURITY_ALERT_VTH_SecPanel))
        mqttc.loop()         
    if WatchdogFlag and mqttc.connected_flag: 
        mqttc.publish(mySecretKeys[ "DZ_IN_TOPIC" ], json.dumps(dzAlarmRAISE_ALARM_FAILURE))        
        mqttc.publish(mySecretKeys[ "DZ_IN_TOPIC" ], json.dumps(WATCHDOG_ALERT_VTH_SecPanel))
        mqttc.loop()           
    if Dahua.VTH_ON_LINE == 0 and not Dahua.SERVICE_SUSPENDED: Dahua.logout()
    if mqttc.connected_flag: mqttc.disconnect()
    stop_threads = True
    if not coldBootP2P: P2P_timeout_Thread.join()
    while Dahua.SERVICE_SUSPENDED: # to avoid program respawn
        time.sleep( 3600 )    
    if WatchdogFlag: sys.exit(WATCHDOG_TERMINATION)
    else: sys.exit(NORMAL_TERMINATION)
