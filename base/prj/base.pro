TEMPLATE = app
CONFIG += console c++14
CONFIG -= app_bundle
CONFIG -= qt

TARGET = base
target.path = .
INSTALLS = target

#sql_files.files = data/configs.sql
#sql_files.files += data/measurements.sql
#sql_files.files += data/sensors.sql
#sql_files.path = .
#INSTALLS += sql_files

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

DEFINES += ASIO_STANDALONE

OBJECTS_DIR = ./.obj/$${DEST_FOLDER}
MOC_DIR = ./.moc/$${DEST_FOLDER}
RCC_DIR = ./.rcc/$${DEST_FOLDER}
UI_DIR = ./.ui/$${DEST_FOLDER}
DESTDIR = ../../../bin/$${DEST_FOLDER}

INCLUDEPATH += ../../sensor/firmware
INCLUDEPATH += ../../common/src/rapidjson/include
INCLUDEPATH += ../../common/src
INCLUDEPATH += ../../common/src/asio/include
INCLUDEPATH += ../src



DISTFILES +=

LIBS += -lpthread

HEADERS += \
    ../src/command.h \
    ../src/pigpio.h \
    ../src/Scheduler.h \
    ../src/Sensors.h \
    ../src/Server.h \
    ../../sensor/firmware/Sensor_Comms.h \
    ../../sensor/firmware/CRC.h \
    ../../sensor/firmware/Storage.h \
    ../src/Log.h \
    ../../common/src/spi.h \
    ../../sensor/firmware/LoRaLib.h \
    ../../sensor/firmware/Module.h \
    ../../sensor/firmware/RFM95.h \
    ../../sensor/firmware/TypeDef.h

SOURCES += \
    ../src/main.cpp \
    ../src/Scheduler.cpp \
    ../src/Sensors.cpp \
    ../src/Server.cpp \
    ../src/tests.cpp \
    ../src/command.c \
    ../src/pigpio.c \
    ../../sensor/firmware/Sensor_Comms.cpp \
    ../../sensor/firmware/CRC.cpp \
    ../../sensor/firmware/Storage.cpp \
    ../../common/src/spi.cpp \
    ../../sensor/firmware/Module.cpp \
    ../../sensor/firmware/RFM95.cpp
