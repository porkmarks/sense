TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

TARGET = server
target.path = .
INSTALLS = target

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

INCLUDEPATH += ../common/src


SOURCES += src/main.cpp \
    ../common/src/Comms.cpp \
    ../common/src/CRC.cpp \
    ../common/src/rfm22b/rfm22b.cpp \
    ../common/src/rfm22b/rfm22b_spi.cpp

HEADERS += \
    ../common/src/Comms.h \
    ../common/src/CRC.h \
    ../common/src/Data_Defs.h \
    ../common/src/rfm22b/rfm22b.h \
    ../common/src/rfm22b/rfm22b_enums.h \
    ../common/src/rfm22b/rfm22b_spi.h

DISTFILES +=


