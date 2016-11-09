CC=gcc
LD=gcc
P_OS ?= linux
B_OBJEXT ?= o

INCLUDES=					\
	-I $(P_PROJDIR)				\
	-I $(P_BASEDIR)/include			\
	-I $(P_BASEDIR)/core/include		\
	-I $(P_BASEDIR)/os/linux/include

EXTRA_CFLAGS +=
ifeq ($(B_DEBUG),0)
EXTRA_CFLAGS += -O2
LDFLAGS =
else
EXTRA_CFLAGS += -g -DDEBUG
LDFLAGS =
endif

all: redfmt redimgbld

%.$(B_OBJEXT): %.c
	$(CC) $(EXTRA_CFLAGS) $(INCLUDES) $< -c -o $@

# The redconf.h for the tools #includes the redconf.h from the parent project
# to inherit its settings, so add it as a dependency.
REDPROJHDR ?= $(P_CONFDIR)/redconf.h

include $(P_BASEDIR)/build/reliance.mk

IMGBLDHDR=							\
	$(P_BASEDIR)/os/linux/tools/imgbld/ibheader.h		\
	$(P_BASEDIR)/os/linux/tools/wintlcmn.h
IMGBLDOBJ=							\
	$(P_BASEDIR)/os/linux/tools/imgbld/ibcommon.$(B_OBJEXT)	\
	$(P_BASEDIR)/os/linux/tools/imgbld/ibfse.$(B_OBJEXT)	\
	$(P_BASEDIR)/os/linux/tools/imgbld/ibposix.$(B_OBJEXT)	\
	$(P_BASEDIR)/os/linux/tools/imgbld/imgbld.$(B_OBJEXT)	\
	$(P_BASEDIR)/os/linux/tools/wintlcmn.$(B_OBJEXT)
REDPROJOBJ=							\
	$(IMGBLDOBJ)						\
	$(P_BASEDIR)/os/linux/tools/imgcopy.$(B_OBJEXT)		\
	$(P_BASEDIR)/os/linux/tools/winchk.$(B_OBJEXT)		\
	$(P_BASEDIR)/os/linux/tools/winfmt.$(B_OBJEXT)

$(P_BASEDIR)/os/linux/tools/imgbld/ibcommon.$(B_OBJEXT):	$(P_BASEDIR)/os/linux/tools/imgbld/ibcommon.c $(REDHDR) $(IMGBLDHDR)
$(P_BASEDIR)/os/linux/tools/imgbld/ibfse.$(B_OBJEXT):		$(P_BASEDIR)/os/linux/tools/imgbld/ibfse.c $(REDHDR) $(IMGBLDHDR)
$(P_BASEDIR)/os/linux/tools/imgbld/ibposix.$(B_OBJEXT):		$(P_BASEDIR)/os/linux/tools/imgbld/ibposix.c $(REDHDR) $(IMGBLDHDR)
$(P_BASEDIR)/os/linux/tools/imgbld/imgbld.$(B_OBJEXT):		$(P_BASEDIR)/os/linux/tools/imgbld/imgbld.c $(REDHDR) $(IMGBLDHDR)
$(P_BASEDIR)/os/linux/tools/winfmt.$(B_OBJEXT):			$(P_BASEDIR)/os/linux/tools/winfmt.c $(REDHDR) $(P_BASEDIR)/os/linux/tools/wintlcmn.h
$(P_BASEDIR)/os/linux/tools/wintlcmn.$(B_OBJEXT):		$(P_BASEDIR)/os/linux/tools/wintlcmn.c  $(REDHDR) $(P_BASEDIR)/os/linux/tools/wintlcmn.h

# The redconf.c for the tools #includes the redconf.c from the parent project
# to inherit its settings, so add it as a dependency.
$(P_PROJDIR)/redconf.$(B_OBJEXT):	$(P_CONFDIR)/redconf.c


redfmt: $(P_BASEDIR)/os/linux/tools/winfmt.$(B_OBJEXT) $(P_BASEDIR)/os/linux/tools/wintlcmn.$(B_OBJEXT) $(REDDRIVOBJ) $(REDTOOLOBJ)
	$(LD) $(LDFLAGS) -o $@ $^

redimgbld: $(IMGBLDOBJ) $(REDDRIVOBJ) $(REDTOOLOBJ)
	$(LD) $(LDFLAGS) -o $@ $^

.phony: clean
clean:
	rm -f $(subst /,\,$(REDDRIVOBJ) $(REDTOOLOBJ) $(REDPROJOBJ)) 2> /dev/null
	rm -f *.ilk *.pdb *.$(B_OBJEXT) *.suo *.sln 2> /dev/null

