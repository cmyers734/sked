
# Use test1 if no TEST (test number) is provided externally
ifeq ($(TEST),)
TARGET := test_basics
else
TARGET := $(TEST)
endif

# Variables that defined our target test environment
MCU = atmega328p
F_CPU = 16000000
FORMAT = ihex
UPLOAD_RATE = 115200

# Name of this Makefile (used for "make depend").
MAKEFILE = Makefile

# Standard Arduino C source includes wiring_* for instance
SRC = $(ARDUINO_C_SOURCE)

CXXSRC = $(ARDUINO_CXX_SOURCE) \
  Sked.cpp \
  tests/$(TARGET).cpp \
  avrcpp.cpp

# Common rules
include common.mk

# Turn on debug
CDEFS += -DSKED_DEBUG=1


lint:
	python ./tools/cpplint.py tests/*.cpp tests/*.h *.cpp


test: build

