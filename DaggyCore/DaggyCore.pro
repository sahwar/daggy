#-------------------------------------------------
#
# Project created by QtCreator 2016-08-04T19:35:23
#
#-------------------------------------------------

TARGET = DaggyCore
TEMPLATE = lib

CONFIG += staticlib

QT += core network

include(../GeneralSettings.pri)

DEFINES += DAGGYCORE_LIBRARY

SOURCES += \
    CDaggy.cpp \
    IRemoteServer.cpp \
    CSshRemoteServer.cpp \
    IRemoteAgregator.cpp \
    CDefaultRemoteServersFabric.cpp \
    CLocalRemoteServer.cpp \
    CDataSourcesFabric.cpp

HEADERS +=\
    Precompiled.h \
    CDaggy.h \
    IRemoteServer.h \
    CSshRemoteServer.h \
    IRemoteServersFabric.h \
    DataSource.h \
    RemoteCommand.h \
    IRemoteAgregator.h \
    CDefaultRemoteServersFabric.h \
    IRemoteAgregatorReciever.h \
    RemoteConnectionStatus.h \
    CLocalRemoteServer.h \
    CDataSourcesFabric.h \
    daggycore_global.h

DEPENDPATH += $$PWD/../ssh

