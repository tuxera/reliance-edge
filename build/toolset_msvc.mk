##
#  Rules for Microsoft Visual Studio tools
##


#  See toolset.mk for documentation on which definitions are required and
#  their intended use.
#
B_CC=cl
B_LIB=lib
B_LIBOUT=/out:
B_OBJEXT ?= obj
B_LIBEXT ?= lib
B_CINCCMD = /I
B_CFLAGS += /W3 /WX /D_CRT_SECURE_NO_WARNINGS $(P_CFLAGS)
ifeq ($(B_DEBUG),0)
B_CFLAGS += /O2 /Ot /Ox
else
B_CFLAGS += /Od /MTd /Od /Zi /RTC1
endif
B_LDCMD=$(B_CC) $(B_CFLAGS) $^ /Fe$@.exe
B_CLEANEXTRA=*.pdb *.ilk

ifeq (, $(shell where $(B_CC)))
$(error "No $(B_CC) in PATH. Make sure you are running from a Visual Studio Command Prompt or you have run vcvarsall.bat")
endif


#  The Win32 port of GNU Make uses sh.exe as its default SHELL.  Change this
#  to cmd.exe.  However, if SHELL has been explicitly assigned a value, leave
#  it alone.
#
ifeq ($(origin SHELL),default)
SHELL=cmd
endif


#  See toolset.mk for documentation on which rules are required and their
#  intended use.
#
%.$(B_OBJEXT): %.c
	$(B_CC) $(B_CFLAGS) $(INCLUDES) $< /c /Fo$@
