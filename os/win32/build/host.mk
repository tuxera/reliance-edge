LD=link
B_OBJEXT ?= tobj

ifeq ($(B_DEBUG),0)
LDFLAGS =
else
LDFLAGS = /DEBUG
endif

.PHONY: all
all: redfmt redimgbld

# The redconf.h for the tools #includes the redconf.h from the parent project
# to inherit its settings, so add it as a dependency.
REDPROJHDR ?= $(P_CONFDIR)/redconf.h

include $(P_BASEDIR)/build/hostos.mk
include $(P_BASEDIR)/build/toolset.mk
include $(P_BASEDIR)/build/reliance.mk

INCLUDES=$(REDDRIVINC)

TOOLHDR=\
	$(P_BASEDIR)/include/redtools.h \
	$(REDTOOLCMNHDR)
IMGBLDOBJ=\
	$(P_BASEDIR)/tools/imgbld/ibfse.$(B_OBJEXT) \
	$(P_BASEDIR)/tools/imgbld/ibposix.$(B_OBJEXT) \
	$(P_BASEDIR)/tools/imgbld/imgbld.$(B_OBJEXT) \
	$(P_BASEDIR)/os/$(P_OS)/tools/imgbldwin.$(B_OBJEXT) \
	$(P_BASEDIR)/os/$(P_OS)/tools/imgbld_main.$(B_OBJEXT) \
	$(REDTOOLCMNOBJ)
REDPROJOBJ=\
	$(IMGBLDOBJ) \
	$(P_BASEDIR)/os/$(P_OS)/tools/$(REDTOOLPREFIX)chk.$(B_OBJEXT) \
	$(P_BASEDIR)/os/$(P_OS)/tools/$(REDTOOLPREFIX)fmt.$(B_OBJEXT)


$(P_BASEDIR)/tools/imgbld/ibcommon.$(B_OBJEXT):		$(P_BASEDIR)/tools/imgbld/ibcommon.c $(REDHDR) $(TOOLHDR)
$(P_BASEDIR)/tools/imgbld/ibfse.$(B_OBJEXT):		$(P_BASEDIR)/tools/imgbld/ibfse.c $(REDHDR) $(TOOLHDR)
$(P_BASEDIR)/tools/imgbld/ibposix.$(B_OBJEXT):		$(P_BASEDIR)/tools/imgbld/ibposix.c $(REDHDR) $(TOOLHDR)
$(P_BASEDIR)/tools/imgbld.$(B_OBJEXT):			$(P_BASEDIR)/tools/imgbld/imgbld.c $(REDHDR) $(TOOLHDR)
$(P_BASEDIR)/os/$(P_OS)/tools/imgbldwin.$(B_OBJEXT):	$(P_BASEDIR)/os/$(P_OS)/tools/imgbldwin.c $(REDHDR) $(TOOLHDR)
$(P_BASEDIR)/os/$(P_OS)/tools/imgbld_main.$(B_OBJEXT):	$(P_BASEDIR)/os/$(P_OS)/tools/imgbld_main.c $(REDHDR) $(TOOLHDR)

# The redconf.c for the tools #includes the redconf.c from the parent project
# to inherit its settings, so add it as a dependency.
$(P_PROJDIR)/redconf.$(B_OBJEXT):	$(P_CONFDIR)/redconf.c


redfmt: $(P_BASEDIR)/os/$(P_OS)/tools/$(REDTOOLPREFIX)fmt.$(B_OBJEXT) $(REDTOOLCMNOBJ) $(REDDRIVOBJ) $(REDTOOLOBJ)
	$(LD) $(LDFLAGS) /OUT:$@.exe $^

redimgbld: $(IMGBLDOBJ) $(REDDRIVOBJ) $(REDTOOLOBJ)
	$(LD) $(LDFLAGS) /OUT:$@.exe $^

.PHONY: clean
clean:
	$(B_DEL) $(subst /,$(B_PATHSEP),$(REDDRIVOBJ) $(REDTOOLOBJ) $(REDPROJOBJ))
	$(B_DEL) $(subst /,$(B_PATHSEP),$(P_BASEDIR)/os/$(P_OS)/tools/*.$(B_OBJEXT))
	$(B_DEL) $(subst /,$(B_PATHSEP),$(P_BASEDIR)/tools/*.$(B_OBJEXT))
	$(B_DEL) $(B_CLEANEXTRA) *.$(B_OBJEXT)
	$(B_DEL) redfmt$(B_EXTEXE) redimgbld$(B_EXTEXE)
