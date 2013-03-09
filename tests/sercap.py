#!/usr/bin/env python

import datetime
import serial
import sys
import time

sentinel = "\x03"
timeout_s = 7

reading = True
timeout = False
s = None
try:
    s = serial.Serial('/dev/tty.usbmodem1421', 115200, timeout=0.1)
    s.flushInput()
    time.sleep(0.5)
    s.write(" ")
    start_reading_ts = datetime.datetime.now()

    while reading:
        d = s.read()
        if len(d) != 0:
            sys.stdout.write(d)

        if sentinel in d:
            reading = False

        now_ts = datetime.datetime.now()
        elapsed = now_ts - start_reading_ts
        if elapsed.total_seconds() > timeout_s:
            reading = False
            timeout = True
finally:
    if s:
        s.close()

if timeout:
    sys.exit(1)
else:
    sys.exit(0)

