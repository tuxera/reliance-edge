##
#  Rules for GNU tools
##


#  See toolset.mk for documentation on which definitions are required and
#  their intended use.
#
B_CC=$(CROSS_COMPILE)gcc
B_LIB=$(CROSS_COMPILE)ar rcsD
B_LIBOUT=
B_OBJEXT ?= o
B_LIBEXT ?= a
B_CINCCMD = -I
B_CFLAGS +=-Wall $(P_CFLAGS)
ifneq ($(B_DEBUG),0)
B_CFLAGS +=-g
endif
B_LDCMD=$(B_CC) $(B_CFLAGS) $^ -o $@
B_CLEANEXTRA=


#  See toolset.mk for documentation on which rules are required and their
#  intended use.
#
%.$(B_OBJEXT): %.c
	$(B_CC) $(B_CFLAGS) $(INCLUDES) -x c -c $< -o $@
