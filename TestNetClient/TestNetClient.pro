#-------------------------------------------------
#
# Project created by QtCreator 2011-11-10T02:36:56
#
#-------------------------------------------------

QT       += core network
QT       -= gui

TARGET = TestNetClient
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += main.cc \
    UnixClient.cc \
    UnixSignalHandler.cc

HEADERS += \
    UnixClient.h \
    UnixSignalHandler.h
