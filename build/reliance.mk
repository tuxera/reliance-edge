##
#  Objects and rules for building Reliance Edge.
##

##
#  Part 1 - the object files.
##

# Objects necessary (in some configuration) to build the file system driver.
REDDRIVOBJ=								\
	$(P_BASEDIR)/bdev/bdev.$(B_OBJEXT)				\
	$(P_BASEDIR)/core/driver/blockio.$(B_OBJEXT)			\
	$(P_BASEDIR)/core/driver/buffer.$(B_OBJEXT)			\
	$(P_BASEDIR)/core/driver/buffercmn.$(B_OBJEXT)			\
	$(P_BASEDIR)/core/driver/core.$(B_OBJEXT)			\
	$(P_BASEDIR)/core/driver/dir.$(B_OBJEXT)			\
	$(P_BASEDIR)/core/driver/format.$(B_OBJEXT)			\
	$(P_BASEDIR)/core/driver/imap.$(B_OBJEXT)			\
	$(P_BASEDIR)/core/driver/imapextern.$(B_OBJEXT)			\
	$(P_BASEDIR)/core/driver/imapinline.$(B_OBJEXT)			\
	$(P_BASEDIR)/core/driver/inode.$(B_OBJEXT)			\
	$(P_BASEDIR)/core/driver/inodedata.$(B_OBJEXT)			\
	$(P_BASEDIR)/core/driver/volume.$(B_OBJEXT)			\
	$(P_BASEDIR)/fse/fse.$(B_OBJEXT)				\
	$(P_BASEDIR)/os/$(P_OS)/services/osassert.$(B_OBJEXT)		\
	$(P_BASEDIR)/os/$(P_OS)/services/osbdev.$(B_OBJEXT)		\
	$(P_BASEDIR)/os/$(P_OS)/services/osclock.$(B_OBJEXT)		\
	$(P_BASEDIR)/os/$(P_OS)/services/osmutex.$(B_OBJEXT)		\
	$(P_BASEDIR)/os/$(P_OS)/services/osoutput.$(B_OBJEXT)		\
	$(P_BASEDIR)/os/$(P_OS)/services/ostask.$(B_OBJEXT)		\
	$(P_BASEDIR)/os/$(P_OS)/services/ostimestamp.$(B_OBJEXT)	\
	$(P_BASEDIR)/posix/path.$(B_OBJEXT)				\
	$(P_BASEDIR)/posix/posix.$(B_OBJEXT)				\
	$(P_BASEDIR)/util/bitmap.$(B_OBJEXT)				\
	$(P_BASEDIR)/util/crc.$(B_OBJEXT)				\
	$(P_BASEDIR)/util/memory.$(B_OBJEXT)				\
	$(P_BASEDIR)/util/namelen.$(B_OBJEXT)				\
	$(P_BASEDIR)/util/sign.$(B_OBJEXT)				\
	$(P_BASEDIR)/util/string.$(B_OBJEXT)				\
	$(P_PROJDIR)/redconf.$(B_OBJEXT)

# Additional objects necessary to build the host tools.
REDTOOLOBJ=								\
	$(P_BASEDIR)/tools/getopt.$(B_OBJEXT)				\
	$(P_BASEDIR)/tools/toolcmn.$(B_OBJEXT)				\
	$(P_BASEDIR)/util/endian.$(B_OBJEXT)

# Additional objects necessary to build the tests.
REDTESTOBJ=								\
	$(P_BASEDIR)/tools/getopt.$(B_OBJEXT)				\
	$(P_BASEDIR)/tools/toolcmn.$(B_OBJEXT)				\
	$(P_BASEDIR)/tests/posix/fmtopt.$(B_OBJEXT)			\
	$(P_BASEDIR)/tests/util/atoi.$(B_OBJEXT)			\
	$(P_BASEDIR)/tests/util/math.$(B_OBJEXT)			\
	$(P_BASEDIR)/tests/util/printf.$(B_OBJEXT)			\
	$(P_BASEDIR)/tests/util/rand.$(B_OBJEXT)
REDTESTOBJ +=								\
	$(P_BASEDIR)/tests/posix/fsstress.$(B_OBJEXT)

# The "sort" function is being used only for its side-effect of removing
# duplicates.  A few object files are listed in more than one of these three
# variables, but duplicates in REDALLOBJ are an issue when this makefile is
# used in certain end-user build environments.
REDALLOBJ=$(sort $(REDDRIVOBJ) $(REDTESTOBJ) $(REDTOOLOBJ))


##
#  Part 2 - the compilation rules.
##

ifndef REDPROJHDR
REDPROJHDR=
endif

REDHDR=							\
	$(P_BASEDIR)/include/redapimacs.h		\
	$(P_BASEDIR)/include/redbdev.h			\
	$(P_BASEDIR)/include/redcoreapi.h		\
	$(P_BASEDIR)/include/rederrno.h			\
	$(P_BASEDIR)/include/redexclude.h		\
	$(P_BASEDIR)/include/redformat.h		\
	$(P_BASEDIR)/include/redfs.h			\
	$(P_BASEDIR)/include/redfse.h			\
	$(P_BASEDIR)/include/redgetopt.h		\
	$(P_BASEDIR)/include/redmacs.h			\
	$(P_BASEDIR)/include/redmisc.h			\
	$(P_BASEDIR)/include/redosserv.h		\
	$(P_BASEDIR)/include/redposix.h			\
	$(P_BASEDIR)/include/redstat.h			\
	$(P_BASEDIR)/include/redtests.h			\
	$(P_BASEDIR)/include/redtestutils.h		\
	$(P_BASEDIR)/include/redtoolcmn.h		\
	$(P_BASEDIR)/include/redutils.h			\
	$(P_BASEDIR)/include/redver.h			\
	$(P_BASEDIR)/include/redvolume.h		\
	$(P_BASEDIR)/os/$(P_OS)/include/redostypes.h	\
	$(P_PROJDIR)/redconf.h				\
	$(P_PROJDIR)/redtypes.h				\
	$(REDPROJHDR)

REDCOREHDR=						\
	$(REDHDR)					\
	$(P_BASEDIR)/core/include/redcore.h		\
	$(P_BASEDIR)/core/include/redcoremacs.h		\
	$(P_BASEDIR)/core/include/redcorevol.h		\
	$(P_BASEDIR)/core/include/rednodes.h


$(P_BASEDIR)/bdev/bdev.$(B_OBJEXT):				$(P_BASEDIR)/bdev/bdev.c $(REDHDR)
$(P_BASEDIR)/core/driver/blockio.$(B_OBJEXT):			$(P_BASEDIR)/core/driver/blockio.c $(REDCOREHDR)
$(P_BASEDIR)/core/driver/buffer.$(B_OBJEXT):			$(P_BASEDIR)/core/driver/buffer.c $(REDCOREHDR) $(P_BASEDIR)/core/driver/redbufferpriv.h
$(P_BASEDIR)/core/driver/buffercmn.$(B_OBJEXT):			$(P_BASEDIR)/core/driver/buffercmn.c $(REDCOREHDR) $(P_BASEDIR)/core/driver/redbufferpriv.h
$(P_BASEDIR)/core/driver/core.$(B_OBJEXT):			$(P_BASEDIR)/core/driver/core.c $(REDCOREHDR)
$(P_BASEDIR)/core/driver/dir.$(B_OBJEXT):			$(P_BASEDIR)/core/driver/dir.c $(REDCOREHDR)
$(P_BASEDIR)/core/driver/format.$(B_OBJEXT):			$(P_BASEDIR)/core/driver/format.c $(REDCOREHDR)
$(P_BASEDIR)/core/driver/imap.$(B_OBJEXT):			$(P_BASEDIR)/core/driver/imap.c $(REDCOREHDR)
$(P_BASEDIR)/core/driver/imapextern.$(B_OBJEXT):		$(P_BASEDIR)/core/driver/imapextern.c $(REDCOREHDR)
$(P_BASEDIR)/core/driver/imapinline.$(B_OBJEXT):		$(P_BASEDIR)/core/driver/imapinline.c $(REDCOREHDR)
$(P_BASEDIR)/core/driver/inode.$(B_OBJEXT):			$(P_BASEDIR)/core/driver/inode.c $(REDCOREHDR)
$(P_BASEDIR)/core/driver/inodedata.$(B_OBJEXT):			$(P_BASEDIR)/core/driver/inodedata.c $(REDCOREHDR)
$(P_BASEDIR)/core/driver/volume.$(B_OBJEXT):			$(P_BASEDIR)/core/driver/volume.c $(REDCOREHDR)
$(P_BASEDIR)/fse/fse.$(B_OBJEXT):				$(P_BASEDIR)/fse/fse.c $(REDHDR)
$(P_BASEDIR)/os/$(P_OS)/services/osassert.$(B_OBJEXT):		$(P_BASEDIR)/os/$(P_OS)/services/osassert.c $(REDHDR)
$(P_BASEDIR)/os/$(P_OS)/services/osbdev.$(B_OBJEXT):		$(P_BASEDIR)/os/$(P_OS)/services/osbdev.c $(REDHDR)
$(P_BASEDIR)/os/$(P_OS)/services/osclock.$(B_OBJEXT):		$(P_BASEDIR)/os/$(P_OS)/services/osclock.c $(REDHDR)
$(P_BASEDIR)/os/$(P_OS)/services/osmutex.$(B_OBJEXT):		$(P_BASEDIR)/os/$(P_OS)/services/osmutex.c $(REDHDR)
$(P_BASEDIR)/os/$(P_OS)/services/osoutput.$(B_OBJEXT):		$(P_BASEDIR)/os/$(P_OS)/services/osoutput.c $(REDHDR)
$(P_BASEDIR)/os/$(P_OS)/services/ostask.$(B_OBJEXT):		$(P_BASEDIR)/os/$(P_OS)/services/ostask.c $(REDHDR)
$(P_BASEDIR)/os/$(P_OS)/services/ostimestamp.$(B_OBJEXT):	$(P_BASEDIR)/os/$(P_OS)/services/ostimestamp.c $(REDHDR)
$(P_BASEDIR)/posix/path.$(B_OBJEXT):				$(P_BASEDIR)/posix/path.c $(REDHDR) $(P_BASEDIR)/include/redpath.h
$(P_BASEDIR)/posix/posix.$(B_OBJEXT):				$(P_BASEDIR)/posix/posix.c $(REDHDR) $(P_BASEDIR)/include/redpath.h
$(P_BASEDIR)/tests/posix/fsstress.$(B_OBJEXT):			$(P_BASEDIR)/tests/posix/fsstress.c $(REDHDR) $(P_BASEDIR)/tests/posix/redposixcompat.h
$(P_BASEDIR)/tests/posix/fmtopt.$(B_OBJEXT):			$(P_BASEDIR)/tests/posix/fmtopt.c $(REDHDR)
$(P_BASEDIR)/tests/util/atoi.$(B_OBJEXT):			$(P_BASEDIR)/tests/util/atoi.c $(REDHDR)
$(P_BASEDIR)/tests/util/math.$(B_OBJEXT):			$(P_BASEDIR)/tests/util/math.c $(REDHDR)
$(P_BASEDIR)/tests/util/printf.$(B_OBJEXT):			$(P_BASEDIR)/tests/util/printf.c $(REDHDR)
$(P_BASEDIR)/tests/util/rand.$(B_OBJEXT):			$(P_BASEDIR)/tests/util/rand.c $(REDHDR)
$(P_BASEDIR)/tools/getopt.$(B_OBJEXT):				$(P_BASEDIR)/tools/getopt.c $(REDHDR)
$(P_BASEDIR)/tools/toolcmn.$(B_OBJEXT):				$(P_BASEDIR)/tools/toolcmn.c $(REDHDR)
$(P_BASEDIR)/util/bitmap.$(B_OBJEXT):				$(P_BASEDIR)/util/bitmap.c $(REDHDR)
$(P_BASEDIR)/util/crc.$(B_OBJEXT):				$(P_BASEDIR)/util/crc.c $(REDHDR)
$(P_BASEDIR)/util/memory.$(B_OBJEXT):				$(P_BASEDIR)/util/memory.c $(REDHDR)
$(P_BASEDIR)/util/namelen.$(B_OBJEXT):				$(P_BASEDIR)/util/namelen.c $(REDHDR)
$(P_BASEDIR)/util/sign.$(B_OBJEXT):				$(P_BASEDIR)/util/sign.c $(REDHDR)
$(P_BASEDIR)/util/string.$(B_OBJEXT):				$(P_BASEDIR)/util/string.c $(REDHDR)
$(P_PROJDIR)/redconf.$(B_OBJEXT):				$(P_PROJDIR)/redconf.c $(REDHDR)

