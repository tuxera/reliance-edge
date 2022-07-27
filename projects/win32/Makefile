P_BASEDIR ?= ../..
P_PROJDIR ?= $(P_BASEDIR)/projects/win32
B_DEBUG ?= 1

.PHONY: all
all: fsstress

include $(P_BASEDIR)/build/hostos.mk
include $(P_BASEDIR)/build/toolset.mk
include $(P_BASEDIR)/build/reliance.mk

INCLUDES=$(REDALLINC)

REDPROJOBJ=
REDPROJOBJ += \
	$(P_PROJDIR)/fsstress_main.$(B_OBJEXT)

$(P_PROJDIR)/fsstress_main.$(B_OBJEXT):		$(P_PROJDIR)/fsstress_main.c $(REDHDR)

fsstress: $(P_PROJDIR)/fsstress_main.$(B_OBJEXT) $(REDALLOBJ)
	$(B_LDCMD)

.PHONY: clean
clean:
	$(B_DEL) *.$(B_OBJEXT) $(B_CLEANEXTRA)
	$(B_DEL) fsstress$(B_EXTEXE)
	$(B_DEL) $(subst /,$(B_PATHSEP),$(REDALLOBJ) $(REDPROJOBJ))
