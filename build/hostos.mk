##
#  Include a host OS specific makefile.
##


#  If B_HOSTOS is unset, set it automatically on supported host OSes
#
ifndef B_HOSTOS
ifeq ($(OS),Windows_NT)
B_HOSTOS=windows
else ifeq ($(shell uname),Linux)
B_HOSTOS=linux
endif
endif


#  Report an error if the host OS makefile does not exist
#
ifeq ($(wildcard $(P_BASEDIR)/build/hostos_$(B_HOSTOS).mk),)
$(error "B_HOSTOS not valid. ex. B_HOSTOS = [windows|linux]")
endif


#  Each host OS makefile should define the following:
#
#  B_RMDIR - Command to remove a directory and its contents.  ex. rmdir
#  B_MKDIR - Command to make a directory.  ex. mkdir
#  B_DEL - Command to delete a file.  ex. rm
#  B_COPY - Command to copy a file.  ex. cp
#  B_PATHSEP - Path separator. ex. /
#  B_SHELL - Command to execute the shell.  ex. sh
#  B_EXTEXE - Executable extension.  ex. .exe
#
include $(P_BASEDIR)/build/hostos_$(B_HOSTOS).mk
