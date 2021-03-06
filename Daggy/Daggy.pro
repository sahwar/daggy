TARGET = daggy
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

QT += core network


include(../GeneralSettings.pri)

SOURCES += main.cpp \
    CApplicationSettings.cpp \
    CConsoleDaggy.cpp \
    CFileDataSourcesReciever.cpp


HEADERS += \
    Precompiled.h \
    CApplicationSettings.h \
    ISystemSignalsHandler.h \
    CConsoleDaggy.h \
    CFileDataSourcesReciever.h


LIBS += -lDaggyCore -lqssh

DAGGY_DESCRIPTION = "Daggy - Data Aggregation Utility. Application that can run multiple commands on remote servers simultaneously and save output locally."

win32: {
    SOURCES += ISystemSignalsHandlerWin32.cpp
    LIBS += -ladvapi32 -luser32 -lws2_32 -lbotan -lyaml-cpp

    RC_ICONS = daggy.ico
    QMAKE_TARGET_DESCRIPTION = $$DAGGY_DESCRIPTION
}

unix: {
    SOURCES += ISystemSignalsHandlerUnix.cpp

    target.path = $$BINDIR
    INSTALLS += target
}


DEPENDPATH += $$PWD/../DaggyCore
DEPENDPATH += $$PWD/../ssh

DEFINES += APP_DESCRIPTION=\"\\\"$${DAGGY_DESCRIPTION}\\\"\"



