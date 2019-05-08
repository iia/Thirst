# Required variables for each Makefile.
# Discard this section from all parent Makefiles.
#
# Expected variables (with automatic defaults):
#
#	[+] CSRCS (all "C" files in the dir).
#
#	[+] SUBDIRS (all subdirs with a Makefile).
#
#	[+] GEN_LIBS
#			List of libs to be generated.
#
#	[+] GEN_IMAGES
#			List of object file images to be generated.
#
#	[+] GEN_BINS
#			List of binaries to be generated.
#
#	[+] COMPONENTS_xxx
#			A list of libs/objs in the form subdir/lib
#			to be extracted and rolled up into a generated
#			lib/image xxx.a.
TARGET = eagle

FLAVOR = debug

#FLAVOR = release

#EXTRA_CCFLAGS += -u

ifndef PDIR
	GEN_IMAGES= eagle.app.v6.out
	GEN_BINS= eagle.app.v6.bin
	SPECIAL_MKTARGETS=$(APP_MKTARGETS)
	SUBDIRS=src
endif

APPDIR = .

LDDIR = ../ld

CCFLAGS += -Os

TARGET_LDFLAGS = \
	-nostdlib \
	-Wl,-EL \
	--longcalls \
	--text-section-literals

ifeq ($(FLAVOR),debug)
	TARGET_LDFLAGS += -g -O2
endif

ifeq ($(FLAVOR),release)
	TARGET_LDFLAGS += -g -O0
endif

COMPONENTS_eagle.app.v6 = src/libthirst.a

LINKFLAGS_eagle.app.v6 = \
	-L../lib \
	-L../libesphttpd \
	-nostdlib \
	-T$(LD_FILE) \
	-Wl,--no-check-sections \
	-Wl,--gc-sections \
	-u call_user_start \
	-Wl,-static \
	-Wl,--start-group \
	-lc \
	-lgcc \
	-lesphttpd \
	-lwebpages-espfs \
	-lphy \
	-lpp \
	-lnet80211 \
	-llwip \
	-lwpa \
	-lcrypto \
	-lmain \
	-ljson \
	-lupgrade \
	-lssl \
	-lpwm \
	-lsmartconfig \
	$(DEP_LIBS_eagle.app.v6) \
	-Wl,--end-group

DEPENDS_eagle.app.v6 = \
	$(LD_FILE) \
	$(LDDIR)/eagle.rom.addr.v6.ld

# Configuration i.e. compile options etc.
# Target specific stuff (defines etc.) goes in here!
#
# Generally values applying to a tree are captured in the
# Makefile at its root level. These are then overridden
# for a subtree within the Makefile rooted therein.
#UNIVERSAL_TARGET_DEFINES = 

# Other potential configuration flags include:
#	-DTXRX_TXBUF_DEBUG
#	-DTXRX_RXBUF_DEBUG
#	-DWLAN_CONFIG_CCX
CONFIGURATION_DEFINES = -DICACHE_FLASH -DGATEWAY_KEY=\"$(GATEWAY_KEY)\"

DEFINES += \
	$(UNIVERSAL_TARGET_DEFINES) \
	$(CONFIGURATION_DEFINES)

DDEFINES += \
	$(UNIVERSAL_TARGET_DEFINES) \
	$(CONFIGURATION_DEFINES)

# Recursion Magic - Don't touch this!!
#
# Each subtree potentially has an include directory
# corresponding to the common APIs applicable to modules
# rooted at that subtree.
#
# Accordingly, the INCLUDE PATH of a module can only contain
# the include directories up its parent path, and not its
# siblings.
#
# Required for each makefile to inherit from the parent.
INCLUDES := $(INCLUDES) -I $(PDIR)include

PDIR := ../$(PDIR)

sinclude $(PDIR)Makefile

.PHONY: FORCE

FORCE:
