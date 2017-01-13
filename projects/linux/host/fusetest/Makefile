#
# makefile for Linux project in Reliance Edge
#
P_PROJDIR ?= $(shell pwd)
P_BASEDIR ?= $(shell readlink -f $(P_PROJDIR)/../../../..)

P_OS     ?= linux
B_OBJEXT ?= o
B_DEBUG  ?= 0
CC       ?= gcc

INCLUDES=					\
	-I $(P_BASEDIR)/include			\
	-I $(P_BASEDIR)/core/include		\
	-I $(P_BASEDIR)/tests/testfw		\
	-I $(P_BASEDIR)/os/linux/include	\
	-I $(P_BASEDIR)/tests/stochposix	\
	-I $(P_PROJDIR)

##
# All warnings are errors, Check to see if the GCC compiler
# options for stack frame and unused variables are supported.
##

EXTRA_CFLAGS +=-Wall
EXTRA_CFLAGS +=-Werror
EXTRA_CFLAGS +=$(call cc-option,-Wframe-larger-than=4096)

ifneq ($(B_DEBUG),0)
EXTRA_CFLAGS +=-g -DDEBUG
endif

all:    osapitest

.c.o:
	$(CC) $(EXTRA_CFLAGS) $(INCLUDES) -DD_DEBUG=$(B_DEBUG) -x c -c $< -o $@

include $(P_BASEDIR)/build/reliance.mk

REDTOSAPIHDR=						\
	$(P_BASEDIR)/tests/os/generic/redtosapiprotos.h	\
	$(P_BASEDIR)/os/linux/include/redtosapiconfig.h

REDTOSAPIOBJ=							\
	$(P_BASEDIR)/tests/os/generic/tosfflush.$(B_OBJEXT)	\
	$(P_BASEDIR)/tests/os/generic/tosfread.$(B_OBJEXT)	\
	$(P_BASEDIR)/tests/os/generic/tosfwrite.$(B_OBJEXT)	\
	$(P_BASEDIR)/tests/os/generic/tosopendir.$(B_OBJEXT)	\
	$(P_BASEDIR)/tests/os/generic/tosremove.$(B_OBJEXT)	\
	$(P_BASEDIR)/tests/os/generic/tosrewinddir.$(B_OBJEXT)	\
	$(P_BASEDIR)/tests/os/generic/tosseekdir.$(B_OBJEXT)	\
	$(P_BASEDIR)/tests/os/generic/tosapi.$(B_OBJEXT)	\
	$(P_BASEDIR)/tests/os/generic/tosfopen.$(B_OBJEXT)	\
	$(P_BASEDIR)/tests/os/generic/tosfseek.$(B_OBJEXT)	\
	$(P_BASEDIR)/tests/os/generic/tosmkdir.$(B_OBJEXT)	\
	$(P_BASEDIR)/tests/os/generic/tosreaddir.$(B_OBJEXT)	\
	$(P_BASEDIR)/tests/os/generic/tosrename.$(B_OBJEXT)	\
	$(P_BASEDIR)/tests/os/generic/tosrmdir.$(B_OBJEXT)

$(P_BASEDIR)/tests/os/generic/tosfflush.$(B_OBJEXT):	$(REDTOSAPIHDR) $(REDHDR)
$(P_BASEDIR)/tests/os/generic/tosfread.$(B_OBJEXT):	$(REDTOSAPIHDR) $(REDHDR)
$(P_BASEDIR)/tests/os/generic/tosfwrite.$(B_OBJEXT):	$(REDTOSAPIHDR) $(REDHDR)
$(P_BASEDIR)/tests/os/generic/tosopendir.$(B_OBJEXT):	$(REDTOSAPIHDR) $(REDHDR)
$(P_BASEDIR)/tests/os/generic/tosremove.$(B_OBJEXT):	$(REDTOSAPIHDR) $(REDHDR)
$(P_BASEDIR)/tests/os/generic/tosrewinddir.$(B_OBJEXT):	$(REDTOSAPIHDR) $(REDHDR)
$(P_BASEDIR)/tests/os/generic/tosseekdir.$(B_OBJEXT):	$(REDTOSAPIHDR) $(REDHDR)
$(P_BASEDIR)/tests/os/generic/tosapi.$(B_OBJEXT):	$(REDTOSAPIHDR) $(REDHDR)
$(P_BASEDIR)/tests/os/generic/tosfopen.$(B_OBJEXT):	$(REDTOSAPIHDR) $(REDHDR)
$(P_BASEDIR)/tests/os/generic/tosfseek.$(B_OBJEXT):	$(REDTOSAPIHDR) $(REDHDR)
$(P_BASEDIR)/tests/os/generic/tosmkdir.$(B_OBJEXT):	$(REDTOSAPIHDR) $(REDHDR)
$(P_BASEDIR)/tests/os/generic/tosreaddir.$(B_OBJEXT):	$(REDTOSAPIHDR) $(REDHDR)
$(P_BASEDIR)/tests/os/generic/tosrename.$(B_OBJEXT):	$(REDTOSAPIHDR) $(REDHDR)
$(P_BASEDIR)/tests/os/generic/tosrmdir.$(B_OBJEXT):	$(REDTOSAPIHDR) $(REDHDR)

osapitest: $(P_PROJDIR)/osapitest_main.$(B_OBJEXT) $(REDTOSAPIOBJ) $(REDDRIVOBJ) $(REDTESTOBJ)
	$(CC) $(EXTRA_CFLAGS) $^ -o $@


.phony: clean
clean:
	-rm -f $(REDTOSAPIOBJ)
	-rm -f $(P_PROJDIR)/*.$(B_OBJEXT)
	-rm -f osapitest

