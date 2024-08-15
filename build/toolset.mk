# Include a specific toolset.

# If B_TOOLSET is unset, default to MSVC on Windows and GCC on Linux.
ifndef B_TOOLSET
ifeq ($(B_HOSTOS),windows)
B_TOOLSET=msvc
else ifeq ($(B_HOSTOS),linux)
B_TOOLSET=gcc
endif
endif

# Set B_DEBUG if it isn't already set.  The toolset makefiles need this in
# order to populate B_CFLAGS.
B_DEBUG ?= 0

# Report an error if the toolset makefile does not exist.
ifeq ($(wildcard $(P_BASEDIR)/build/toolset_$(B_TOOLSET).mk),)
$(error "B_TOOLSET not valid.")
endif

# Each toolset should define the following:
#
# B_CC - Command used to compile C code.
# B_OBJEXT - File extension for object files (e.g., obj or o).
# B_CINCCMD - Compiler option for an include directory (e.g., /I or -I).
# B_CFLAGS - Compiler options and defines.
# B_LDCMD - Linker invocation command (e.g., $(B_CC) $(B_CFLAGS) $^ -o $@).
# B_CLEANEXTRA - Extra file extensions to delete in clean targets.
#
# Each toolset should have the following rules:
#
# %.$(B_OBJEXT): %.c - Rule for compiling C code.
include $(P_BASEDIR)/build/toolset_$(B_TOOLSET).mk
