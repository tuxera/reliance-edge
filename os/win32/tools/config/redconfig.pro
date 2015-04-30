#-------------------------------------------------
#
# Project created by QtCreator 2015-02-06T15:40:36
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = redconfig
TEMPLATE = app

SOURCES += main.cpp\
        configwindow.cpp \
    cbsetting.cpp \
    warningbtn.cpp \
    cmbintsetting.cpp \
    cmbstrsetting.cpp \
    sbsetting.cpp \
    rbtnsetting.cpp \
    boolsetting.cpp \
    intsetting.cpp \
    strsetting.cpp \
    pathsepsetting.cpp \
    volumesettings.cpp \
    validators.cpp \
    allsettings.cpp \
    lesetting.cpp \
    output.cpp \
    errordialog.cpp \
    application.cpp \
    input.cpp \
    filedialog.cpp \
    limitreporter.cpp

HEADERS  += configwindow.h \
    setting.h \
    cbsetting.h \
    warningbtn.h \
    validity.h \
    cmbintsetting.h \
    cmbstrsetting.h \
    sbsetting.h \
    rbtnsetting.h \
    boolsetting.h \
    intsetting.h \
    strsetting.h \
    pathsepsetting.h \
    volumesettings.h \
    validators.h \
    debug.h \
    allsettings.h \
    lesetting.h \
    settingbase.h \
    output.h \
    errordialog.h \
    application.h \
    input.h \
    filedialog.h \
    notifiable.h \
    limitreporter.h

FORMS    += configwindow.ui \
    warningbtn.ui \
    errordialog.ui

RESOURCES += \
    icons.qrc

DISTFILES += \
    icon-error-16.png \
    icon-warning-16.png

 RC_FILE = ic.rc
