P_BASEDIR ?= ../..
P_PROJDIR ?= $(P_BASEDIR)/projects/win32
B_DEBUG ?= 1

CC=cl
P_OS ?= win32
B_OBJEXT ?= obj
SHELL=cmd

INCLUDES=					\
	/I $(P_BASEDIR)/include			\
	/I $(P_BASEDIR)/core/include		\
	/I $(P_BASEDIR)/tests/testfw		\
	/I $(P_BASEDIR)/tests/stochposix	\
	/I $(P_BASEDIR)/os/win32/include	\
	/I $(P_BASEDIR)/os/win32/tools		\
	/I $(P_PROJDIR)

EXTRA_CFLAGS += /W3 /WX /D_CRT_SECURE_NO_WARNINGS
ifeq ($(B_DEBUG),0)
EXTRA_CFLAGS += /O2 /Ot /Ox
else
EXTRA_CFLAGS += /Od /DD_DEBUG /MTd /Od /Zi /RTC1
endif

ifeq (, $(shell where $(CC)))
$(error "No $(CC) in PATH. Make sure you are running from a Visual Studio Command Prompt or you have run vcvarsall.bat")
endif

all: fsstress

%.$(B_OBJEXT): %.c
	$(CC) $(EXTRA_CFLAGS) -DD_DEBUG=$(B_DEBUG) $(INCLUDES) $< /c /Fo$@

include $(P_BASEDIR)/build/reliance.mk

REDPROJOBJ=							\
	$(P_BASEDIR)/os/win32/tools/wintlcmn.$(B_OBJEXT)
REDPROJOBJ +=							\
	$(P_PROJDIR)/fsstress_main.$(B_OBJEXT)

$(P_PROJDIR)/fsstress_main.$(B_OBJEXT):			$(P_PROJDIR)/fsstress_main.c $(REDHDR) $(P_BASEDIR)/os/win32/tools/wintlcmn.h
$(P_BASEDIR)/os/win32/tools/wintlcmn.$(B_OBJEXT):	$(P_BASEDIR)/os/win32/tools/wintlcmn.c $(REDHDR) $(P_BASEDIR)/os/win32/tools/wintlcmn.h

fsstress: $(P_PROJDIR)/fsstress_main.$(B_OBJEXT) $(P_BASEDIR)/os/win32/tools/wintlcmn.$(B_OBJEXT) $(REDALLOBJ)
	$(CC) $(EXTRA_CFLAGS) $^ /Fe$@.exe

.phony: clean
clean:
	del /Q/F *.$(B_OBJEXT) *.pdb *.ilk *.exe
	del /Q/F $(subst /,\,$(REDALLOBJ) $(REDPROJOBJ))

