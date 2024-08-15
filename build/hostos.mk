# Include a host OS specific makefile.

# If B_HOSTOS is unset, set it automatically on supported host OSes.
ifndef B_HOSTOS
ifeq ($(OS),Windows_NT)
B_HOSTOS=windows
else ifeq ($(shell uname),Linux)
B_HOSTOS=linux
endif
endif

# Report an error if the host OS makefile does not exist.
ifeq ($(wildcard $(P_BASEDIR)/build/hostos_$(B_HOSTOS).mk),)
$(error "B_HOSTOS not valid.")
endif

# Each host OS makefile should define the following:
#
# B_DEL - Command to delete a file (e.g., rm).
# B_PATHSEP - Path separator (e.g., /).
# B_EXTEXE - Executable extension (e.g., .exe).
include $(P_BASEDIR)/build/hostos_$(B_HOSTOS).mk
