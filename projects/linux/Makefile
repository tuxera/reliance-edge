#
# makefile for Linux project in Reliance Edge
#
P_PROJDIR ?= $(shell pwd)
P_BASEDIR ?= $(shell readlink -f $(P_PROJDIR)/../..)

P_OS ?= linux
B_OBJEXT ?= o
B_DEBUG ?= 1
CC ?= gcc

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

all: fsstress

.c.o:
	$(CC) $(EXTRA_CFLAGS) $(INCLUDES) -DD_DEBUG=$(B_DEBUG) -x c -c $< -o $@

include $(P_BASEDIR)/build/reliance.mk

REDPROJOBJ=						\
	$(P_PROJDIR)/fsstress_main.$(B_OBJEXT)

$(P_PROJDIR)/fsstress_main.$(B_OBJEXT):		$(P_PROJDIR)/fsstress_main.c $(REDHDR)

fsstress: $(P_PROJDIR)/fsstress_main.$(B_OBJEXT) $(REDALLOBJ)
	$(CC) $(EXTRA_CFLAGS) $^ -o $@

.phony: clean
clean:
	-rm -f $(REDALLOBJ) $(REDPROJOBJ)
	-rm -f $(P_PROJDIR)/*.$(B_OBJEXT)
	-rm -f fsstress

