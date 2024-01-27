#
# makefile for Linux project in Reliance Edge
#
P_BASEDIR ?= ../..
P_PROJDIR ?= $(P_BASEDIR)/projects/linux

B_DEBUG ?= 1

P_CFLAGS +=-Werror
ifneq ($(B_DEBUG),0)
P_CFLAGS +=-DDEBUG
endif

.PHONY: all
all: fsstress

include $(P_BASEDIR)/build/hostos.mk
include $(P_BASEDIR)/build/toolset.mk
include $(P_BASEDIR)/build/reliance.mk

INCLUDES=$(REDALLINC)

REDPROJOBJ=\
	$(P_PROJDIR)/fsstress_main.$(B_OBJEXT)

$(P_PROJDIR)/fsstress_main.$(B_OBJEXT):		$(P_PROJDIR)/fsstress_main.c $(REDHDR)

fsstress: $(P_PROJDIR)/fsstress_main.$(B_OBJEXT) $(REDALLOBJ)
	$(B_LDCMD)

.PHONY: clean
clean:
	$(B_DEL) $(REDALLOBJ) $(REDPROJOBJ)
	$(B_DEL) $(P_PROJDIR)/*.$(B_OBJEXT)
	$(B_DEL) fsstress
