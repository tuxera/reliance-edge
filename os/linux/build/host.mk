CC=gcc
P_OS ?= linux
B_OBJEXT ?= to

INCLUDES=					\
	-I $(P_PROJDIR)				\
	-I $(P_BASEDIR)/include			\
	-I $(P_BASEDIR)/core/include		\
	-I $(P_BASEDIR)/os/linux/include

EXTRA_CFLAGS +=-Wall
EXTRA_CFLAGS +=-Werror
EXTRA_CFLAGS +=-Wno-switch-break
EXTRA_CFLAGS +=-Wno-pointer-to-int-cast
EXTRA_CFLAGS +=$(call cc-option,-Wframe-larger-than=4096)
EXTRA_CFLAGS +=$(call cc-option,-Wno-error=unused-variable)

ifneq ($(B_DEBUG),0)
EXTRA_CFLAGS += -g -DDEBUG
endif

all: redfmt redimgbld

%.$(B_OBJEXT): %.c
	$(CC) $(EXTRA_CFLAGS) $(INCLUDES) -DD_DEBUG=$(B_DEBUG) -D_XOPEN_SOURCE=500 -x c -c $< -o $@

# The redconf.h for the tools #includes the redconf.h from the parent project
# to inherit its settings, so add it as a dependency.
REDPROJHDR=$(P_CONFDIR)/redconf.h

include $(P_BASEDIR)/build/reliance.mk

TOOLHDR=							\
	$(P_BASEDIR)/include/redtools.h
IMGBLDOBJ=							\
	$(P_BASEDIR)/tools/imgbld/ibcommon.$(B_OBJEXT)		\
	$(P_BASEDIR)/tools/imgbld/ibfse.$(B_OBJEXT)		\
	$(P_BASEDIR)/tools/imgbld/ibposix.$(B_OBJEXT)		\
	$(P_BASEDIR)/tools/imgbld/imgbld.$(B_OBJEXT)		\
	$(P_BASEDIR)/os/linux/tools/imgbldlinux.$(B_OBJEXT)	\
	$(P_BASEDIR)/os/linux/tools/imgbld_main.$(B_OBJEXT)
REDPROJOBJ=							\
	$(IMGBLDOBJ)						\
	$(P_BASEDIR)/os/linux/tools/linuxchk.$(B_OBJEXT)	\
	$(P_BASEDIR)/os/linux/tools/linuxfmt.$(B_OBJEXT)



$(P_BASEDIR)/tools/imgbld/ibcommon.$(B_OBJEXT):		$(P_BASEDIR)/tools/imgbld/ibcommon.c $(REDHDR) $(TOOLHDR)
$(P_BASEDIR)/tools/imgbld/ibfse.$(B_OBJEXT):		$(P_BASEDIR)/tools/imgbld/ibfse.c $(REDHDR) $(TOOLHDR)
$(P_BASEDIR)/tools/imgbld/ibposix.$(B_OBJEXT):		$(P_BASEDIR)/tools/imgbld/ibposix.c $(REDHDR) $(TOOLHDR)
$(P_BASEDIR)/tools/imgbld.$(B_OBJEXT):			$(P_BASEDIR)/tools/imgbld/imgbld.c $(REDHDR) $(TOOLHDR)
$(P_BASEDIR)/os/linux/tools/imgbldlinux.$(B_OBJEXT):	$(P_BASEDIR)/os/linux/tools/imgbldlinux.c $(REDHDR) $(TOOLHDR)
$(P_BASEDIR)/os/linux/tools/imgbld_main.$(B_OBJEXT):	$(P_BASEDIR)/os/linux/tools/imgbld_main.c $(REDHDR) $(TOOLHDR)
$(P_BASEDIR)/os/linux/tools/linuxfmt.$(B_OBJEXT):	$(P_BASEDIR)/os/linux/tools/linuxfmt.c $(REDHDR)

# The redconf.c for the tools #includes the redconf.c from the parent project
# to inherit its settings, so add it as a dependency.
$(P_PROJDIR)/redconf.$(B_OBJEXT):	$(P_CONFDIR)/redconf.c


redfmt: $(P_BASEDIR)/os/linux/tools/linuxfmt.$(B_OBJEXT) $(REDDRIVOBJ) $(REDTOOLOBJ)
	$(CC) $(EXTRA_CFLAGS) $^ -o $@

redimgbld: $(IMGBLDOBJ) $(REDDRIVOBJ) $(REDTOOLOBJ)
	$(CC) $(EXTRA_CFLAGS) $^ -o $@

.phony: clean
clean:
	-rm -f $(REDDRIVOBJ) $(REDTOOLOBJ) $(REDPROJOBJ)
	-rm -f $(P_BASEDIR)/os/linux/tools/*.$(B_OBJEXT)
	-rm -f $(P_BASEDIR)/tools/*.$(B_OBJEXT)
	-rm -f redfmt redimgbld


