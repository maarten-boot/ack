# ======================================================================= #
#                          ACK CONFIGURATION                              #
#                      (Edit this before building)                        #
# ======================================================================= #

# What platform to build for by default?

DEFAULT_PLATFORM ?= minix68k

# Which architectures should get built?

# (Can also be given on the command line, e.g. make PLATS="minix68k pc86":
# build/ab.mk regenerates the build when it changes.)

PLATS ?= minix68k
# PLATS = linux386 linuxppc linuxmips

# Where the build goes. The build runs commands in sandboxes inside OBJ
# with paths relative to the source tree (build/_sandbox.py), so OBJ must
# be a relative path inside it.

$(if $(filter /% ~%,$(OBJ))$(findstring ..,$(OBJ)), \
	$(error OBJ=$(OBJ): must be a relative path inside the source tree))

# Where should the ACK put its temporary files?

ifeq ($(TMPDIR),)
ACK_TEMP_DIR ?= /tmp
else
ACK_TEMP_DIR ?= $(TMPDIR)
endif

INSDIR = $(OBJ)/staging

# Where is the ACK going to be installed, eventually? If you don't want to
# install it and just want to run the ACK from the build directory
# ($(TMPDIR)/ack-build/staging, by default), leave this as $(INSDIR).

ifeq ($(OS),Windows_NT)
PREFIX ?= c:/ack
else
PREFIX ?= /opt/pkg/ack
#PREFIX = $(INSDIR)
endif

# What build flags do you want to use for native code?

CFLAGS ?= -g -Os \
	-fno-strict-aliasing \
	-Werror=return-type \
	-Werror=implicit-function-declaration \
	-Werror=strict-prototypes \
	-DUNREACHABLE_CODE='__builtin_unreachable()' \
	-DNORETURN=_Noreturn

HOSTCFLAGS = $(CFLAGS)
ACKCFLAGS = -O

LDFLAGS ?= -g
ACKLDFLAGS =

# Various commands.

LUA ?= lua

.PHONY: all
all: +all

# Custom rule to build the installer.

ack-setup.exe: etc/windows-installer.nsi all
	makensis -dBUILDDIR="$$(realpath $(INSDIR))" -dOUTFILE="$$(realpath $@)" $<

install: all
	@mkdir -p $(PREFIX)
	tar cf - -C $(INSDIR) . | tar xvf - -C $(PREFIX)

PLATIND = $(INSDIR)/share/ack
PLATDEP = $(INSDIR)/lib/ack
export PLATS

AB_ENABLE_PROGRESS_INFO = false
include build/ab.mk
