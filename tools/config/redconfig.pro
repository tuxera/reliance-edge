#-------------------------------------------------
#
# Project created by QtCreator 2015-02-06T15:40:36
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = redconfig
TEMPLATE = app

INCLUDEPATH += $$PWD
INCLUDEPATH += $$PWD/include
# Because Qt designer isn't smart enough to #include "ui/....h"
INCLUDEPATH += $$PWD/ui

SOURCES += main.cpp\
    validators.cpp \
    allsettings.cpp \
    ui/configwindow.cpp \
    ui/errordialog.cpp \
    ui/filedialog.cpp \
    ui/warningbtn.cpp \
    settings/boolsetting.cpp \
    settings/cbsetting.cpp \
    settings/cmbintsetting.cpp \
    settings/cmbstrsetting.cpp \
    settings/intsetting.cpp \
    settings/lesetting.cpp \
    settings/pathsepsetting.cpp \
    settings/rbtnsetting.cpp \
    settings/sbsetting.cpp \
    settings/strsetting.cpp \
    control/output.cpp \
    control/input.cpp \
    control/application.cpp \
    settings/dindirreporter.cpp \
    settings/limitreporter.cpp \
    volumesettings.cpp

HEADERS  += \
    validators.h \
    allsettings.h \
    ui/configwindow.h \
    ui/warningbtn.h \
    ui/filedialog.h \
    ui/errordialog.h \
    settings/cbsetting.h \
    settings/boolsetting.h \
    settings/cmbintsetting.h \
    settings/cmbstrsetting.h \
    settings/intsetting.h \
    settings/lesetting.h \
    settings/pathsepsetting.h \
    settings/rbtnsetting.h \
    settings/sbsetting.h \
    settings/setting.h \
    settings/settingbase.h \
    settings/strsetting.h \
    control/output.h \
    control/input.h \
    control/application.h \
    settings/dindirreporter.h \
    settings/limitreporter.h \
    settings/notifiable.h \
    include/debug.h \
    include/validity.h \
    include/version.h \
    volumesettings.h

FORMS    += \
    ui/configwindow.ui \
    ui/errordialog.ui \
    ui/warningbtn.ui

RESOURCES += \
    res/icons.qrc

DISTFILES += \
    res/icon-error-16.png \
    res/icon-warning-16.png

RC_FILE = res/ic.rc

CONFIG += c++11

linux-g++* {
    QMAKE_CXX = g++-4.8
    QMAKE_CC = gcc-4.8

    # This project contains many object initializers in class constructors that
    # have side effects. However, the order of initialization of these objects
    # is not significant. Thus, the Wreorder warning can be ignored.
    QMAKE_CXXFLAGS += -Wno-reorder
}
