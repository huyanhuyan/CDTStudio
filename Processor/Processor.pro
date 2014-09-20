#-------------------------------------------------
#
# Project created by QtCreator 2014-09-10T16:24:41
#
#-------------------------------------------------

QT       += core network xml xmlpatterns
QT       -= gui

CONFIG   += console
CONFIG   -= app_bundle

TARGET = Processor
DESTDIR = ../bin

TEMPLATE = app


SOURCES += main.cpp \
    cdtprocessorapplication.cpp \
    cdttask.cpp \
    cdttaskmanager.cpp \
    cdttask_pbcdbinary.cpp \
    messagehandler.cpp

HEADERS += \
    cdtprocessorapplication.h \
    cdttask.h \
    cdttaskmanager.h \
    cdttask_pbcdbinary.h \
    messagehandler.h

INCLUDEPATH += \
    ../Interfaces \
    ../Tools\

unix{
QMAKE_CXXFLAGS += -std=c++0x
LIBS += -lgdal

INCLUDEPATH += /usr/include/gdal \
/usr/local/include/gdal \
}
!unix{
include(../Tools/Config/win.pri)
LIBS += -lgdal_i
}

LIBS += -L../lib -llog4qt

RESOURCES += \
    resource.qrc

