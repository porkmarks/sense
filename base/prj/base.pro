TEMPLATE = app
CONFIG += console c++11
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
    ../../common/src/rfm22b_spi.h \
    ../../sensor/firmware/rfm22b.h \
    ../../sensor/firmware/rfm22b_enums.h \
    ../../sensor/firmware/Sensor_Comms.h \
    ../../sensor/firmware/CRC.h \
    ../../sensor/firmware/Storage.h \
    ../src/Log.h

SOURCES += \
    ../src/main.cpp \
    ../src/Scheduler.cpp \
    ../src/Sensors.cpp \
    ../src/Server.cpp \
    ../src/tests.cpp \
    ../src/command.c \
    ../src/pigpio.c \
    ../../common/src/rfm22b_spi.cpp \
    ../../sensor/firmware/rfm22b.cpp \
    ../../sensor/firmware/Sensor_Comms.cpp \
    ../../sensor/firmware/CRC.cpp \
    ../../sensor/firmware/Storage.cpp
