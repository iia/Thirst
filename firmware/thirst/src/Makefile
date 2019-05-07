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
#		List of libs to be generated.
#
#	[+] GEN_IMAGES
#		List of object file images to be generated.
#
#	[+] GEN_BINS
#		List of binaries to be generated.
#
#	[+] COMPONENTS_xxx
#		A list of libs/objs in the form subdir/lib
#		to be extracted and rolled up into a generated
#		lib/image xxx.a.
ifndef PDIR
	GEN_LIBS = libthirst.a
endif

# Configuration i.e. compile options etc.
# Target specific stuff (defines etc.) goes in here!
#
# Generally values applying to a tree are captured in the
# Makefile at its root level. These are then overridden
# for a subtree within the Makefile rooted therein.
#DEFINES +=

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

INCLUDES += -I ./

INCLUDES += -I ../../include/ets

INCLUDES += -I ../../libesphttpd/include

PDIR := ../$(PDIR)

include $(PDIR)Makefile
