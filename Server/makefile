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

# Stuf for install
BINFOLD = /opt/
CONFOLD = /etc/
LOGFOLD = /var/log/
FOLDNAME = DBSS/

all: $(TARGET)

$(TARGET): $(SRCFOLD)$(TARGET).cpp
	@echo "Compiling DBSS..."
	$(CC) $(CPPFLAGS) -o $(OUTFOLD)$(TARGET) $(SRCFOLD)$(TARGET).cpp
	@echo "Done."

clean:
	rm -R bin/*

install: $(TARGET)
	@echo "Moving binary to" $(BINFOLD)$(FOLDNAME)
	test -d $(BINFOLD)$(FOLDNAME) || mkdir $(BINFOLD)$(FOLDNAME)
	cp $(OUTFOLD)$(TARGET) $(BINFOLD)$(FOLDNAME)$(TARGET)
	@echo "Done."
	@echo "Creating config folder" $(CONFOLD)$(FOLDNAME)
	test -d $(CONFOLD)$(FOLDNAME) || mkdir $(CONFOLD)$(FOLDNAME)
	@echo "Done."
	@echo "Creating log folder" $(LOGFOLD)$(FOLDNAME)
	test -d $(LOGFOLD)$(FOLDNAME) || mkdir $(LOGFOLD)$(FOLDNAME)
	@echo "Done."
	@echo "Initializing config and log files..."
	$(BINFOLD)$(FOLDNAME)$(TARGET) -i -c $(CONFOLD)$(FOLDNAME)DBSS_CONF.cnf --log $(LOGFOLD)$(FOLDNAME)DBSS_LOG.txt
	@echo "Done."
	@echo "Setting up to run as service..."
	
	@echo ""
	@echo "Installation Complete."

uninstall:
	@echo "Removing Binary..."
	test ! -d $(BINFOLD)$(FOLDNAME) || rm -R $(BINFOLD)$(FOLDNAME)
	@echo "Done. To remove Config and Log, use: 'uninstall-all'"

uninstall-all: uninstall
	@echo "Removing Config folder..."
	rm -R $(CONFOLD)$(FOLDNAME)
	@echo "Done."
	@echo "Removing Log Folder..."
	rm -R $(LOGFOLD)$(FOLDNAME)
	@echo "Done."
	@echo "Done Removing Config and Log"

