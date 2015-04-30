P_BASEDIR ?= ../..
P_PROJDIR ?= $(P_BASEDIR)/projects/win32
B_DEBUG ?= 1

CC=cl
P_OS ?= win32
B_OBJEXT ?= obj

INCLUDES=					\
	/I $(P_BASEDIR)/include			\
	/I $(P_BASEDIR)/core/include		\
	/I $(P_BASEDIR)/tests/testfw		\
	/I $(P_BASEDIR)/tests/stochposix	\
	/I $(P_BASEDIR)/os/win32/include	\
	/I $(P_BASEDIR)/os/win32/tools		\
	/I $(P_PROJDIR)

EXTRA_CFLAGS += /W3 /D_CRT_SECURE_NO_WARNINGS
ifeq ($(B_DEBUG),0)
EXTRA_CFLAGS += /O2 /Ot /Ox
else
EXTRA_CFLAGS += /Od /DD_DEBUG /MTd /Od /Zi /RTC1
endif

all: fsstress

%.$(B_OBJEXT): %.c
	$(CC) $(EXTRA_CFLAGS) -DD_DEBUG=$(B_DEBUG) $(INCLUDES) $< /c /Fo$@

include $(P_BASEDIR)/build/reliance.mk

REDPROJOBJ=							\
	$(P_BASEDIR)/os/win32/tools/wintlcmn.$(B_OBJEXT)
REDPROJOBJ +=							\
	$(P_PROJDIR)/fsstress_main.$(B_OBJEXT)

$(P_PROJDIR)/fsstress_main.$(B_OBJEXT):			$(P_PROJDIR)/fsstress_main.c $(REDHDR)
$(P_BASEDIR)/os/win32/tools/wintlcmn.$(B_OBJEXT):	$(P_BASEDIR)/os/win32/tools/wintlcmn.c $(P_BASEDIR)/os/win32/tools/wintlcmn.h

fsstress: $(P_PROJDIR)/fsstress_main.$(B_OBJEXT) $(P_BASEDIR)/os/win32/tools/wintlcmn.$(B_OBJEXT) $(REDALLOBJ)
	$(CC) $(EXTRA_CFLAGS) $^ /Fe$@.exe

.phony: clean
clean:
	-cmd /c del /Q/F *.$(B_OBJEXT) *.pdb *.ilk *.exe *.suo *.sln
	-cmd /c del /Q/F $(subst /,\,$(REDALLOBJ) $(REDPROJOBJ))

