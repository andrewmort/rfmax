#
# File: Makefile
# Author: Andrew Mort
# Description: Top Level Makefile
# 		Based on Makefile from github.com/ruuvi/ruuvitag_fw
# 
# Version:
# 	11/15/2017:
# 			Initial Creation
#

# Top of project
TOP := $(shell pwd)

# SDK Information
SDK_VERSION := 14.2.0_17b948a
SDK_URL     := https://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v14.x.x
SDK_FILE    := nRF5_SDK_$(SDK_VERSION).zip

SDK_DIR     := $(basename $(SDK_FILE))
SDK_HOME    := $(TOP)/$(SDK_DIR)

export $(SDK_HOME)

# ARM GCC Information for SDK
SDK_MAKEFILE := $(SDK_HOME)/components/toolchain/gcc/Makefile.posix
GNU_PREFIX   := arm-none-eabi
GNU_GCC      := $(GNU_PREFIX)-gcc
GNU_DIR      := $(dir $(shell which $(GNU_GCC)))
GREP_CMD     := grep -Po '([0-9]+([.][0-9]+)+)'
GNU_VERSION  := $(shell $(GNU_GCC) --version | $(GREP_CMD))

# Download/Unzip Commands
DOWNLOAD_CMD := curl -o
UNZIP_CMD    := unzip -q -d

.PHONY: all

all: $(SDK_MAKEFILE)

# Unzip SDK Directory
$(SDK_DIR): $(SDK_FILE)
	@echo unzipping SDK...
	$(UNZIP_CMD) $(SDK_DIR) $(SDK_FILE)

# Download SDK File
$(SDK_FILE):
	@echo downloading SDK zip...
	$(DOWNLOAD_CMD) $(SDK_FILE) $(SDK_URL)/$(SDK_FILE)

# Fix paths and names of arm gcc in SDK Makefile.posix file
$(SDK_MAKEFILE): $(SDK_DIR)
	@echo fixing paths in SDK...
	@echo "GNU_INSTALL_ROOT := $(GNU_DIR)" > $(SDK_MAKEFILE)
	@echo "GNU_VERSION := $(GNU_VERSION)" >> $(SDK_MAKEFILE)
	@echo "GNU_PREFIX := $(GNU_PREFIX)" >> $(SDK_MAKEFILE)

blinky:
	$(MAKE) -C projects/blinky/pca10040/blank/armgcc SDK_ROOT=$(SDK_HOME)

base:
	$(MAKE) -C projects/base/pca10040/blank/armgcc SDK_ROOT=$(SDK_HOME)
