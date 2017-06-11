# Path definitions.
FW_BASE = bin
MODULES = src
SDK_LDDIR = ld
SDK_LIBDIR = lib
BUILD_BASE = build
EXTRA_INCDIR = src/include
LIBESPHTTPD_INCDIR = include
# Base directory of the SDK.
SDK_BASE ?= esp-open-sdk/sdk
SDK_INCDIR = include driver_lib/include
# Base directory of the compiler toolchain.
XTENSA_TOOLS_ROOT ?= esp-open-sdk/xtensa-lx106-elf/bin

# SDK version to use.
SDK_VER = 2.0.0

LIBESPHTTPD_BASE = libesphttpd

# esptool related definitions.
ESPTOOL ?= esptool.py
ESPPORT ?= /dev/ttyUSB0

# Project name.
TARGET = Thirst
TARGET_VER = 1.0.0

# Libraries used. Mostly from the SDK.
LIBS = c gcc hal pp phy net80211 lwip wpa main esphttpd webpages-espfs

# Compilation flags.
CFLAGS = -Os -g -O2 -Wpointer-arith -Wundef -Werror -Wl,-EL \
-fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals \
-D__ets__ -DICACHE_FLASH

# Linker flags.
LDFLAGS = -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static

# The linker script to use.
LD_SCRIPT = -Tld/Thirst_4MB.ld

# Binary files and their address on the flash chip.
BLANK_ADDR = 0x3FE000
APP_CODE_ADDR = 0x00000
SDK_CODE_ADDR = 0x40000
INIT_DATA_ADDR = 0x3FC000
MASTER_DEVICE_KEY_ADDR = 0x3E000
BLANK_BIN = $(SDK_BASE)/bin/blank.bin
MASTER_DEVICE_KEY_BIN = $(BLANK_BIN)
APP_CODE_BIN = $(FW_BASE)/$(APP_CODE_ADDR).bin
SDK_CODE_BIN = $(FW_BASE)/$(SDK_CODE_ADDR).bin
INIT_DATA_BIN = $(SDK_BASE)/bin/esp_init_data_default.bin

# Compilation tools.
CC := $(XTENSA_TOOLS_ROOT)/xtensa-lx106-elf-gcc
AR := $(XTENSA_TOOLS_ROOT)/xtensa-lx106-elf-ar
LD := $(XTENSA_TOOLS_ROOT)/xtensa-lx106-elf-gcc

# Nothing much to configure.
# Everything is derived from already defined variables.
SRC_DIR		:= $(MODULES)
BUILD_DIR	:= $(addprefix $(BUILD_BASE)/,$(MODULES))

SDK_LIBDIR	:= $(addprefix $(SDK_BASE)/,$(SDK_LIBDIR))
SDK_INCDIR	:= $(addprefix -I$(SDK_BASE)/,$(SDK_INCDIR))

LIBESPHTTPD_LIBDIR := $(LIBESPHTTPD_BASE)/
LIBESPHTTPD_INCDIR := $(addprefix -I$(LIBESPHTTPD_BASE)/,$(LIBESPHTTPD_INCDIR))

SRC					:= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
OBJ					:= $(patsubst %.c,$(BUILD_BASE)/%.o,$(SRC))
LIBS				:= $(addprefix -l,$(LIBS))
APP_AR			:= $(addprefix $(BUILD_BASE)/,$(TARGET)_app.a)
TARGET_OUT	:= $(addprefix $(BUILD_BASE)/,$(TARGET).out)

INCDIR				:= $(addprefix -I,$(SRC_DIR))
EXTRA_INCDIR	:= $(addprefix -I,$(EXTRA_INCDIR))
MODULE_INCDIR	:= $(addsuffix /include,$(INCDIR))

V ?= $(VERBOSE)
ifeq ("$(V)","1")
Q :=
vecho := @true
else
Q := @
vecho := @echo
endif

vpath %.c $(SRC_DIR)

define compile-objects
$1/%.o: %.c
	$(vecho) "CC $$<"
	$(Q) $(CC) $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) \
	$(LIBESPHTTPD_INCDIR) $(CFLAGS) -c $$< -o $$@
endef

.PHONY: all checkdirs flash prepare get-libesphttpd tc-build \
	compile-libesphttpd clean

all: checkdirs compile-libesphttpd $(TARGET_OUT) $(APP_CODE_BIN) $(SDK_CODE_BIN)

$(FW_BASE)/%.bin: $(TARGET_OUT) | $(FW_BASE)
	$(vecho) "FW $(FW_BASE)/"
	$(Q) $(ESPTOOL) elf2image -o $(FW_BASE)/ $(TARGET_OUT)

$(TARGET_OUT): $(APP_AR)
	$(vecho) "LD $@"
	$(Q) $(LD) -L$(SDK_LIBDIR) -L$(LIBESPHTTPD_LIBDIR) $(LD_SCRIPT) $(LDFLAGS) \
	-Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $@

$(APP_AR): $(OBJ)
	$(vecho) "AR $@"
	$(Q) $(AR) cru $@ $^

checkdirs: $(BUILD_DIR) $(FW_BASE)

$(BUILD_DIR):
	$(Q) mkdir -p $@

$(FW_BASE):
	$(Q) mkdir -p $@

flash: $(APP_CODE_BIN) $(SDK_CODE_BIN)
	$(ESPTOOL) --port $(ESPPORT) --baud 2000000 write_flash --flash_mode qio \
	--flash_size 32m --flash_freq 80m $(APP_CODE_ADDR) $(APP_CODE_BIN) $(SDK_CODE_ADDR) \
	$(SDK_CODE_BIN) $(MASTER_DEVICE_KEY_ADDR) $(MASTER_DEVICE_KEY_BIN) \
	$(INIT_DATA_ADDR) $(INIT_DATA_BIN) $(BLANK_ADDR) $(BLANK_BIN)

prepare: get-libesphttpd tc-build

compile-libesphttpd:
	$(MAKE) -C $(LIBESPHTTPD_BASE) clean
	$(MAKE) -C $(LIBESPHTTPD_BASE) USE_OPENSDK=yes SDK_BASE=../$(SDK_BASE)/ \
	XTENSA_TOOLS_ROOT=../$(XTENSA_TOOLS_ROOT)/ HTTPD_MAX_CONNECTIONS=8 \
	GZIP_COMPRESSION=no COMPRESS_W_YUI=yes YUI-COMPRESSOR=/usr/bin/yui-compressor \
	USE_HEATSHRINK=yes HTTPD_WEBSOCKETS=no HTMLDIR=../src/html

get-libesphttpd:
	$(Q) rm -rf libesphttpd; git clone https://github.com/Spritetm/libesphttpd.git

tc-build:
	$(Q) rm -rf esp-open-sdk; \
	git clone --recursive https://github.com/pfalcon/esp-open-sdk.git
	cd esp-open-sdk && $(MAKE) STANDALONE=y VENDOR_SDK=$(SDK_VER)

clean:
	$(Q) rm -rf $(FW_BASE) $(BUILD_BASE)

$(foreach bdir,$(BUILD_DIR),$(eval $(call compile-objects,$(bdir))))
