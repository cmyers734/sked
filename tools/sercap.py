#!/usr/bin/env python
#
# Script to capture serial port output during unit testing.
# 
# Copyright (c) 2013, Christopher Myers.  All Rights Reserved.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#

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

