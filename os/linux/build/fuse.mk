CC ?= gcc
P_OS ?= linux
B_OBJEXT ?= to

INCLUDES=					\
	-I $(P_PROJDIR)				\
	-I $(P_BASEDIR)/include			\
	-I $(P_BASEDIR)/core/include		\
	-I $(P_BASEDIR)/os/linux/include

LDFLAGS = -lfuse

EXTRA_CFLAGS +=-Wall
EXTRA_CFLAGS +=-Werror
EXTRA_CFLAGS +=$(call cc-option,-Wframe-larger-than=4096)
EXTRA_CFLAGS +=$(call cc-option,-Wno-error=unused-variable)
EXTRA_CFLAGS += -D_FILE_OFFSET_BITS=64

ifneq ($(B_DEBUG),0)
EXTRA_CFLAGS += -g -DDEBUG
endif

all: reliance_fuse

%.$(B_OBJEXT): %.c
	$(CC) $(EXTRA_CFLAGS) $(INCLUDES) -DD_DEBUG=$(B_DEBUG) -D_XOPEN_SOURCE=500 -x c -c $< -o $@

# The redconf.h for the tools #includes the redconf.h from the parent project
# to inherit its settings, so add it as a dependency.
REDPROJHDR ?= $(P_CONFDIR)/redconf.h

include $(P_BASEDIR)/build/reliance.mk

# The redconf.c for the tools #includes the redconf.c from the parent project
# to inherit its settings, so add it as a dependency.
$(P_PROJDIR)/redconf.$(B_OBJEXT):	$(P_CONFDIR)/redconf.c

reliance_fuse: $(P_PROJDIR)/reliance_fuse.$(B_OBJEXT) $(REDDRIVOBJ)
	$(CC) $^ $(LDFLAGS) -o $@

.phony: clean
clean:
	rm -f $(REDDRIVOBJ) $(REDTOOLOBJ) $(REDPROJOBJ)
	rm -f reliance_fuse
	rm -f *.$(B_OBJEXT)

