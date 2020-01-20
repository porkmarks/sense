QT += gui network

CONFIG += c++11 console
CONFIG -= app_bundle
CONFIG += c++17

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += ../../src
INCLUDEPATH += ../../src/tests
INCLUDEPATH += ../../src/sqlite
INCLUDEPATH += ../../src/Smtp
INCLUDEPATH += ../../../common/src
INCLUDEPATH += ../../../sensor/firmware

linux {
    LIBS += -ldl
}

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ../../src/DB.cpp \
    ../../src/sqlite/sqlite3.c \
    ../../src/Utils.cpp \
    ../../src/Emailer.cpp \
    ../../src/Smtp/smtpclient.cpp \
    ../../src/Smtp/quotedprintable.cpp \
    ../../src/Smtp/mimetext.cpp \
    ../../src/Smtp/mimepart.cpp \
    ../../src/Smtp/mimemultipart.cpp \
    ../../src/Smtp/mimemessage.cpp \
    ../../src/Smtp/mimeinlinefile.cpp \
    ../../src/Smtp/mimehtml.cpp \
    ../../src/Smtp/mimefile.cpp \
    ../../src/Smtp/mimecontentformatter.cpp \
    ../../src/Smtp/mimeattachment.cpp \
    ../../src/Smtp/emailaddress.cpp \
    ../../src/Logger.cpp \
    ../../src/tests/testMain.cpp \
    ../../src/tests/testUtils.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    ../../src/DB.h \
    ../../src/sqlite/sqlite3ext.h \
    ../../src/sqlite/sqlite3.h \
    ../../src/Utils.h \
    ../../src/Emailer.h \
    ../../src/Smtp/smtpexports.h \
    ../../src/Smtp/smtpclient.h \
    ../../src/Smtp/quotedprintable.h \
    ../../src/Smtp/mimetext.h \
    ../../src/Smtp/mimepart.h \
    ../../src/Smtp/mimemultipart.h \
    ../../src/Smtp/mimemessage.h \
    ../../src/Smtp/mimeinlinefile.h \
    ../../src/Smtp/mimehtml.h \
    ../../src/Smtp/mimefile.h \
    ../../src/Smtp/mimecontentformatter.h \
    ../../src/Smtp/mimeattachment.h \
    ../../src/Smtp/emailaddress.h \
    ../../src/Smtp/SmtpMime \
    ../../src/Logger.h \
    ../../src/tests/testUtils.h
