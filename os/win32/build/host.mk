LD=link
B_OBJEXT ?= tobj

ifeq ($(B_DEBUG),0)
LDFLAGS =
else
LDFLAGS = /DEBUG
endif

.PHONY: default
default: redfmt redimgbld

.PHONY: all
all: default

# The redconf.h for the tools #includes the redconf.h from the parent project
# to inherit its settings, so add it as a dependency.
REDPROJHDR ?= $(P_CONFDIR)/redconf.h

include $(P_BASEDIR)/build/hostos.mk
include $(P_BASEDIR)/build/toolset.mk
include $(P_BASEDIR)/build/reliance.mk

INCLUDES=$(REDDRIVINC)

ifneq ($(DokanLibrary2),)
INCLUDES += \
	$(B_CINCCMD) "${DokanLibrary2}include" \
	$(B_CINCCMD) "${DokanLibrary2}include\fuse"

B_CFLAGS += /Zc:wchar_t

all: edge-u
endif

TOOLHDR=\
	$(P_BASEDIR)/include/redtools.h
IMGBLDOBJ=\
	$(P_BASEDIR)/tools/imgbld/ibfse.$(B_OBJEXT) \
	$(P_BASEDIR)/tools/imgbld/ibposix.$(B_OBJEXT) \
	$(P_BASEDIR)/tools/imgbld/imgbld.$(B_OBJEXT) \
	$(P_BASEDIR)/os/$(P_OS)/tools/imgbldwin.$(B_OBJEXT) \
	$(P_BASEDIR)/os/$(P_OS)/tools/imgbld_main.$(B_OBJEXT)
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
$(P_BASEDIR)/tools/fuse.$(B_OBJEXT):			$(P_BASEDIR)/tools/fuse.c $(REDHDR)

# The redconf.c for the tools #includes the redconf.c from the parent project
# to inherit its settings, so add it as a dependency.
$(P_PROJDIR)/redconf.$(B_OBJEXT):	$(P_CONFDIR)/redconf.c

redfmt: $(P_BASEDIR)/os/$(P_OS)/tools/$(REDTOOLPREFIX)fmt.$(B_OBJEXT) $(REDDRIVOBJ) $(REDTOOLOBJ)
	$(LD) $(LDFLAGS) /OUT:$@.exe $^

redimgbld: $(IMGBLDOBJ) $(REDDRIVOBJ) $(REDTOOLOBJ)
	$(LD) $(LDFLAGS) /OUT:$@.exe $^

ifneq ($(DokanLibrary2),)
# FUSE driver reimplements the UID/GID OS service in fuse.c.
REDFUSEDRIVOBJ := $(subst $(P_BASEDIR)/os/$(P_OS)/services/osuidgid.$(B_OBJEXT),,$(REDDRIVOBJ))

ifeq ($(shell cl 2>&1 | findstr "x86"),)
DOKANFUSE_LIB = "${DokanLibrary2_LibraryPath_x64}dokanfuse2.lib"
else
DOKANFUSE_LIB = "${DokanLibrary2_LibraryPath_x86}dokanfuse2.lib"
endif

edge-u: $(P_BASEDIR)/tools/fuse.$(B_OBJEXT) $(REDTOOLOBJ) $(REDFUSEDRIVOBJ)
	$(LD) $(LDFLAGS) /DEFAULTLIB:$(DOKANFUSE_LIB) /OUT:$@.exe $^
else
edge-u:
	$(error edge-u requires Dokany to be installed: %DokanLibrary2% must be set)
endif

# redfuse was renamed to edge-u but retain the redfuse target for backward
# compatibility.
redfuse: edge-u
	rename $^.exe $@.exe

.PHONY: clean
clean:
	$(B_DEL) $(subst /,$(B_PATHSEP),$(REDDRIVOBJ) $(REDTOOLOBJ) $(REDPROJOBJ))
	$(B_DEL) $(subst /,$(B_PATHSEP),$(P_BASEDIR)/os/$(P_OS)/tools/*.$(B_OBJEXT))
	$(B_DEL) $(subst /,$(B_PATHSEP),$(P_BASEDIR)/tools/*.$(B_OBJEXT))
	$(B_DEL) $(B_CLEANEXTRA) *.$(B_OBJEXT)
	$(B_DEL) edge-u$(B_EXTEXE) redfmt$(B_EXTEXE) redfuse$(B_EXTEXE) redimgbld$(B_EXTEXE)
