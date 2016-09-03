#!/usr/bin/make -f
# Makefile to compile the Server
#	Greg Stewart 2016

# compiler
CC = g++

# flags:
#   -std=c++0x Turns on c++ v2011 support
CPPFLAGS = -std=c++0x

# The build target executable
TARGET = DecentBackupServer
SRCFOLD = src/
OUTFOLD = bin/

all: $(TARGET)

$(TARGET): src/$(TARGET).cpp
	$(CC) $(CPPFLAGS) -o $(OUTFOLD)$(TARGET) $(SRCFOLD)$(TARGET).cpp   

clean:
	rm -R bin/*
