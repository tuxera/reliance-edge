CC=gcc
LD=gcc
P_OS ?= linux
B_OBJEXT ?= to

INCLUDES=					\
	-I $(P_PROJDIR)				\
	-I $(P_BASEDIR)/include			\
	-I $(P_BASEDIR)/core/include		\
	-I $(P_BASEDIR)/os/linux/include

EXTRA_CFLAGS += -D_FILE_OFFSET_BITS=64
ifeq ($(B_DEBUG),0)
EXTRA_CFLAGS += -O2
LDFLAGS = -lfuse
else
EXTRA_CFLAGS += -g -DDEBUG
LDFLAGS = -lfuse
endif

all: reliance_fuse

%.$(B_OBJEXT): %.c
	$(CC) $(EXTRA_CFLAGS) $(INCLUDES) $< -c -o $@

# The redconf.h for the tools #includes the redconf.h from the parent project
# to inherit its settings, so add it as a dependency.
REDPROJHDR ?= $(P_CONFDIR)/redconf.h

include $(P_BASEDIR)/build/reliance.mk

# The redconf.c for the tools #includes the redconf.c from the parent project
# to inherit its settings, so add it as a dependency.
$(P_PROJDIR)/redconf.$(B_OBJEXT):	$(P_CONFDIR)/redconf.c

reliance_fuse: $(P_PROJDIR)/reliance_fuse.$(B_OBJEXT) $(IMGBLDOBJ) $(REDDRIVOBJ) $(REDTOOLOBJ)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDFLAGS)

.phony: clean
clean:
	rm -f $(REDDRIVOBJ) $(REDTOOLOBJ) $(REDPROJOBJ) 2> /dev/null
	rm -f reliance_fuse  2> /dev/null
	rm -f *.$(B_OBJEXT) 2> /dev/null

