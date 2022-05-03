from asyncio import base_tasks
from cgi import test
import os
import io
import time
from traceback import print_stack
import serial
import signal
import sys
import ntpath
import string
import csv
import glob
import socket
from optparse import OptionParser
import json
from datetime import datetime
PORT='/dev/ttyACM2'
cmd1="fs ls 0x0\r"
cmd2="tmp 0x0\r"
ser = serial.Serial(PORT)  # open serial port
ser.port=PORT
ser.baudrate=115200
ser.parity=serial.PARITY_NONE
ser.stopbits=serial.STOPBITS_ONE
ser.bytesize = serial.EIGHTBITS #number of bits per bytes
ser.xonxoff = False             #disable software flow control
ser.rtscts = False              #disable hardware (RTS/CTS) flow control
ser.dsrdtr = False              #disable hardware (DSR/DTR) flow control
ser.timeout = 0                 #non-block read
ser.writeTimeout = 2          #timeout for write+
ser.exclusive = True
sio = io.TextIOWrapper(io.BufferedRWPair(ser,ser),line_buffering=True)
# print(fs_cmd_prefix)
#sio.write(cmd)     # write a string
flag=True
def testing():
    global flag
    cntr=0
    total_response= ''
    if flag:
        flag=False
        sio.write(cmd1)
    else:
        flag=True
        sio.write(cmd2)
    while True:
        ret_line = sio.readline()
        if cntr == 10000:
            print('res '+str(cntr),repr(total_response))
            return -1
        total_response+=ret_line 
        # print('res',repr(total_response))
        if 'Status: 0x1' in total_response:
            print('res '+str(cntr),repr(total_response))
            return -1
        if 'CM>' in total_response:
            is_success = True
            print('res '+str(cntr),repr(total_response))
            break
        time.sleep(0.001)
        cntr=cntr+1
	

for i in range (0,1000):
    rslt=testing()
    if rslt==-1:
        print("FAIL after "+str(i)+"!!")
        exit()
    else:
        print("pkt #"+str(i))
