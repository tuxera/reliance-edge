P_BASEDIR ?= ../..
P_PROJDIR ?= $(P_BASEDIR)/projects/linux
B_DEBUG ?= 1

CC=gcc
P_OS ?= linux
B_OBJEXT ?= o

INCLUDES=					\
	-I $(P_BASEDIR)/include			\
	-I $(P_BASEDIR)/core/include		\
	-I $(P_BASEDIR)/tests/testfw		\
	-I $(P_BASEDIR)/tests/stochposix	\
	-I $(P_BASEDIR)/os/linux/include	\
	-I $(P_BASEDIR)/os/linux/tools		\
	-I $(P_PROJDIR)

EXTRA_CFLAGS +=
ifeq ($(B_DEBUG),0)
EXTRA_CFLAGS += -O2
else
EXTRA_CFLAGS += -g -DDEBUG
endif

all: fsstress

%.$(B_OBJEXT): %.c
	$(CC) $(EXTRA_CFLAGS) $(INCLUDES) $< -c -o $@

include $(P_BASEDIR)/build/reliance.mk

REDPROJOBJ=							\
	$(P_BASEDIR)/os/linux/tools/wintlcmn.$(B_OBJEXT)
REDPROJOBJ +=							\
	$(P_PROJDIR)/fsstress_main.$(B_OBJEXT)

$(P_PROJDIR)/fsstress_main.$(B_OBJEXT):			$(P_PROJDIR)/fsstress_main.c $(REDHDR) $(P_BASEDIR)/os/linux/tools/wintlcmn.h
$(P_BASEDIR)/os/linux/tools/wintlcmn.$(B_OBJEXT):	$(P_BASEDIR)/os/linux/tools/wintlcmn.c $(REDHDR) $(P_BASEDIR)/os/linux/tools/wintlcmn.h

fsstress: $(P_PROJDIR)/fsstress_main.$(B_OBJEXT) $(P_BASEDIR)/os/linux/tools/wintlcmn.$(B_OBJEXT) $(REDALLOBJ)
	$(CC) $(EXTRA_CFLAGS) $^ -o $@

.phony: clean
clean:
	rm -f *.$(B_OBJEXT) 2> /dev/null
	rm -f $(REDALLOBJ) $(REDPROJOBJ) 2> /dev/null
	rm -f fsstress 2> /dev/null

