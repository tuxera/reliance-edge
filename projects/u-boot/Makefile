#
# makefile for u-boot project in Reliance Edge
#
P_BASEDIR ?= fs/redfs/red
P_PROJDIR ?= $(P_BASEDIR)/projects/u-boot

P_OS     ?= u-boot
B_OBJEXT ?= o
B_DEBUG  ?= 0
CC       ?= gcc

INCLUDES=					\
	-I $(P_BASEDIR)/include			\
	-I $(P_BASEDIR)/core/include		\
	-I $(P_BASEDIR)/os/u-boot/include	\
	-I $(P_PROJDIR)

##
# All warnings are errors, Check to see if the GCC compiler
# options for stack frame and unused variables are supported.
##

EXTRA_CFLAGS +=-Wall
EXTRA_CFLAGS +=-Werror
EXTRA_CFLAGS +=-Wno-date-time
EXTRA_CFLAGS +=$(call cc-option,-Wframe-larger-than=4096)
EXTRA_CFLAGS +=$(INCLUDES)

ifneq ($(B_DEBUG),0)
EXTRA_CFLAGS +=-g -DDEBUG
endif

#include $(P_BASEDIR)/build/reliance.mk

REDPROJOBJ= \
	red/core/driver/blockio.$(B_OBJEXT)			\
	red/core/driver/buffer.$(B_OBJEXT)			\
	red/core/driver/core.$(B_OBJEXT)			\
	red/core/driver/dir.$(B_OBJEXT)				\
	red/core/driver/format.$(B_OBJEXT)			\
	red/core/driver/imap.$(B_OBJEXT)			\
	red/core/driver/imapextern.$(B_OBJEXT)			\
	red/core/driver/imapinline.$(B_OBJEXT)			\
	red/core/driver/inode.$(B_OBJEXT)			\
	red/core/driver/inodedata.$(B_OBJEXT)			\
	red/core/driver/volume.$(B_OBJEXT)			\
	red/fse/fse.$(B_OBJEXT)					\
	red/os/$(P_OS)/services/osassert.$(B_OBJEXT)		\
	red/os/$(P_OS)/services/osbdev.$(B_OBJEXT)		\
	red/os/$(P_OS)/services/osclock.$(B_OBJEXT)		\
	red/os/$(P_OS)/services/osmutex.$(B_OBJEXT)		\
	red/os/$(P_OS)/services/osoutput.$(B_OBJEXT)		\
	red/os/$(P_OS)/services/ostask.$(B_OBJEXT)		\
	red/os/$(P_OS)/services/ostimestamp.$(B_OBJEXT)		\
	red/posix/path.$(B_OBJEXT)				\
	red/posix/posix.$(B_OBJEXT)				\
	red/util/bitmap.$(B_OBJEXT)				\
	red/util/crc.$(B_OBJEXT)				\
	red/util/memory.$(B_OBJEXT)				\
	red/util/namelen.$(B_OBJEXT)				\
	red/util/sign.$(B_OBJEXT)				\
	red/util/string.$(B_OBJEXT)				\
	red/projects/u-boot/redconf.$(B_OBJEXT)			\
	red/os/$(P_OS)/vfs/vfs.$(B_OBJEXT)


.phony: clean
clean:
	-rm -f $(REDALLOBJ) $(REDPROJOBJ)
	-rm -f $(P_PROJDIR)/*.$(B_OBJEXT)

