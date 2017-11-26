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

OBJECTS_DIR = ./.obj/$${DEST_FOLDER}
MOC_DIR = ./.moc/$${DEST_FOLDER}
RCC_DIR = ./.rcc/$${DEST_FOLDER}
UI_DIR = ./.ui/$${DEST_FOLDER}
DESTDIR = ../../bin/$${DEST_FOLDER}

INCLUDEPATH += ../sensor/firmware
INCLUDEPATH += ../common/src/rapidjson/include


SOURCES += src/main.cpp \
    src/tests.cpp \
    src/Scheduler.cpp \
    src/Sensors.cpp \
    src/Server.cpp

HEADERS += \
    src/Scheduler.h \
    src/Sensors.h \
    src/Server.h \

DISTFILES +=

LIBS += -lboost_thread -lboost_system -lpthread -lmysqlpp -lmysqlclient -lpigpio
