##
#  Defines for a Windows host operating system
##


#  See hostos.mk for documentation on which definitions are required and
#  their intended use.
#
B_RMDIR = rmdir /s /q
B_MKDIR = mkdir
B_DEL = del /Q/F
B_COPY = copy
B_PATHSEP = $(subst /,\,/)
B_SHELL = cmd
B_BASH = "C:\\Program Files\\Git\\bin\\bash.exe"
B_EXTEXE = .exe
