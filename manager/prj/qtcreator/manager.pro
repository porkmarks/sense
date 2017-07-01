#-------------------------------------------------
#
# Project created by QtCreator 2014-06-04T18:33:20
#
#-------------------------------------------------

QT       += core gui opengl network widgets charts quick qml quickwidgets positioning

TARGET = manager
TEMPLATE = app

#TARGET = manager
#target.path = /home/jean/fpv/manager
#INSTALLS = target

CONFIG += console
CONFIG += c++11

INCLUDEPATH += ../../src
INCLUDEPATH += ../../../common/src
INCLUDEPATH += ../../../common/src/rapidjson/include

QMAKE_CXXFLAGS += -Wno-unused-variable -Wno-unused-parameter -B$HOME/dev/bin/gold
QMAKE_CFLAGS += -Wno-unused-variable -Wno-unused-parameter -B$HOME/dev/bin/gold

PRECOMPILED_HEADER = ../../src/stdafx.h
CONFIG *= precompile_header

ROOT_LIBS_PATH = ../../../..

rpi {
    DEFINES+=RASPBERRY_PI
    CONFIG(debug, debug|release) {
        DEST_FOLDER = rpi/debug
    }
    CONFIG(release, debug|release) {
        DEST_FOLDER = rpi/release
        DEFINES += NDEBUG
    }
} else {
    CONFIG(debug, debug|release) {
        DEST_FOLDER = pc/debug
    }
    CONFIG(release, debug|release) {
        DEST_FOLDER = pc/release
        DEFINES += NDEBUG
    }
}

OBJECTS_DIR = ./.obj/$${DEST_FOLDER}
MOC_DIR = ./.moc/$${DEST_FOLDER}
RCC_DIR = ./.rcc/$${DEST_FOLDER}
UI_DIR = ./.ui/$${DEST_FOLDER}
DESTDIR = ../../bin

RESOURCES += \
    ../../res/res.qrc

HEADERS += \
    ../../src/Manager.h \
    ../../src/stdafx.h \
    ../../src/Comms.h \
    ../../src/BaseStationsWidget.h \
    ../../src/ConfigWidget.h \
    ../../src/MeasurementsWidget.h \
    ../../src/AlarmsWidget.h \
    ../../../common/src/Channel.h \
    ../../../common/src/QTcpSocketAdapter.h \
    ../../../common/src/CRC.h \
    ../../src/SensorsWidget.h \
    ../../src/SensorsModel.h \
    ../../src/AlarmsModel.h \
    ../../src/MeasurementsModel.h \
    ../../src/DB.h

SOURCES += \
    ../../src/Manager.cpp \
    ../../src/main.cpp \
    ../../src/Comms.cpp \
    ../../src/BaseStationsWidget.cpp \
    ../../src/ConfigWidget.cpp \
    ../../src/MeasurementsWidget.cpp \
    ../../src/AlarmsWidget.cpp \
    ../../../common/src/CRC.cpp \
    ../../src/DB.cpp \
    ../../src/SensorsWidget.cpp \
    ../../src/SensorsModel.cpp \
    ../../src/AlarmsModel.cpp \
    ../../src/MeasurementsModel.cpp

FORMS += \
    ../../src/Manager.ui \
    ../../src/BaseStationsWidget.ui \
    ../../src/ConfigWidget.ui \
    ../../src/MeasurementsWidget.ui \
    ../../src/SensorsWidget.ui \
    ../../src/SensorsFilterDialog.ui \
    ../../src/AlarmsWidget.ui \
    ../../src/NewAlarmDialog.ui

DISTFILES += \

