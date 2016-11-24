#
# Makefile for a Reliance Edge Win32 host tools project
#
P_BASEDIR ?= ../../../../..
P_PROJDIR ?= ./
P_CONFDIR ?= $(P_PROJDIR)/../src
B_DEBUG ?= 1

ifeq ($(OS),Windows_NT)
include $(P_BASEDIR)/os/win32/build/host.mk
else
include $(P_BASEDIR)/os/linux/build/host.mk
endif

