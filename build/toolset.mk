##
#  Include a specific toolset.
##


#  If B_TOOLSET is unset, default to MSVC on Windows and GCC on Linux
#
ifndef B_TOOLSET
ifeq ($(B_HOSTOS),windows)
B_TOOLSET=msvc
else ifeq ($(B_HOSTOS),linux)
B_TOOLSET=gcc
endif
endif

#  Set B_DEBUG if it isn't already set.  The toolset makefiles need this in
#  order to populate B_CFLAGS.
#
B_DEBUG ?= 0


#  Report an error if the toolset makefile does not exist
#
ifeq ($(wildcard $(P_BASEDIR)/build/toolset_$(B_TOOLSET).mk),)
$(error "B_TOOLSET not valid. ex. B_TOOLSET = [msvc|gcc]")
endif


#  Each toolset should define the following:
#
#  B_CC - Command used to compile c code
#  B_LIB - Command used to create a library
#  B_LIBOUT - LIB option for specifying the output library.  ex. /out:
#  B_OBJEXT - File extension for object files.  ex. obj, o
#  B_LIBEXT - File extension for library files.  ex. lib, a
#  B_CINCCMD - Compiler option for an include directory.  ex. /I, -i
#  B_CFLAGS - Compiler options and defines.
#  B_LDCMD - Linker invocation command.  ex. $(B_CC) $(B_CFLAGS) $^ -o $@
#  B_CLEANEXTRA - Extra file extensions to delete in clean targets.
#
#  Each toolset should have the following rules:
#
#  %.$(B_OBJEXT): %.c - Rule for compiling c code
#
#  These defines are optional:
#
#  P_CFLAGS - Project-specific compiler options (e.g., predefined macros)
#
include $(P_BASEDIR)/build/toolset_$(B_TOOLSET).mk
