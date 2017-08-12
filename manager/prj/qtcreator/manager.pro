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
CONFIG -= console

INCLUDEPATH += ../../src
INCLUDEPATH += ../../src/Smtp
INCLUDEPATH += ../../src/qftp
INCLUDEPATH += ../../../common/src
INCLUDEPATH += ../../../common/src/rapidjson/include

QMAKE_CXXFLAGS += -Wno-unused-variable -Wno-unused-parameter
QMAKE_CFLAGS += -Wno-unused-variable -Wno-unused-parameter
#QMAKE_LFLAGS += -Wl,-subsystem,windows

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
    ../../src/MeasurementsWidget.h \
    ../../src/AlarmsWidget.h \
    ../../../common/src/Channel.h \
    ../../../common/src/QTcpSocketAdapter.h \
    ../../../common/src/CRC.h \
    ../../src/SensorsWidget.h \
    ../../src/SensorsModel.h \
    ../../src/AlarmsModel.h \
    ../../src/MeasurementsModel.h \
    ../../src/DB.h \
    ../../src/MeasurementsDelegate.h \
    ../../src/SensorsDelegate.h \
    ../../src/ConfigureAlarmDialog.h \
    ../../src/PlotWidget.h \
    ../../../common/src/Butterworth.h \
    ../../src/ExportDataDialog.h \
    ../../src/ExportPicDialog.h \
    ../../src/ReportsModel.h \
    ../../src/ReportsWidget.h \
    ../../src/ConfigureReportDialog.h \
    ../../src/Utils.h \
    ../../../common/src/Crypt.h \
    ../../src/Emailer.h \
    ../../src/SensorSettingsDialog.h \
    ../../src/EmailSettingsDialog.h \
    ../../src/FtpSettingsDialog.h \
    ../../src/Smtp/emailaddress.h \
    ../../src/Smtp/mimeattachment.h \
    ../../src/Smtp/mimecontentformatter.h \
    ../../src/Smtp/mimefile.h \
    ../../src/Smtp/mimehtml.h \
    ../../src/Smtp/mimeinlinefile.h \
    ../../src/Smtp/mimemessage.h \
    ../../src/Smtp/mimemultipart.h \
    ../../src/Smtp/mimepart.h \
    ../../src/Smtp/mimetext.h \
    ../../src/Smtp/quotedprintable.h \
    ../../src/Smtp/smtpclient.h \
    ../../src/Smtp/smtpexports.h \
    ../../src/SettingsWidget.h \
    ../../src/ConfigureUserDialog.h \
    ../../src/UsersModel.h \
    ../../src/UsersWidget.h \
    ../../src/Settings.h \
    ../../src/LogsWidget.h \
    ../../src/LogsModel.h \
    ../../src/Logger.h \
    ../../src/ExportLogsDialog.h \
    ../../src/PlotToolTip.h \
    ../../src/qftp/qftp.h \
    ../../src/qftp/qurlinfo.h

SOURCES += \
    ../../src/Manager.cpp \
    ../../src/main.cpp \
    ../../src/Comms.cpp \
    ../../src/BaseStationsWidget.cpp \
    ../../src/MeasurementsWidget.cpp \
    ../../src/AlarmsWidget.cpp \
    ../../../common/src/CRC.cpp \
    ../../src/DB.cpp \
    ../../src/SensorsWidget.cpp \
    ../../src/SensorsModel.cpp \
    ../../src/AlarmsModel.cpp \
    ../../src/MeasurementsModel.cpp \
    ../../src/MeasurementsDelegate.cpp \
    ../../src/SensorsDelegate.cpp \
    ../../src/ConfigureAlarmDialog.cpp \
    ../../src/PlotWidget.cpp \
    ../../src/ExportDataDialog.cpp \
    ../../src/ExportPicDialog.cpp \
    ../../src/ReportsModel.cpp \
    ../../src/ReportsWidget.cpp \
    ../../src/ConfigureReportDialog.cpp \
    ../../src/Utils.cpp \
    ../../../common/src/Crypt.cpp \
    ../../src/Emailer.cpp \
    ../../src/SensorSettingsDialog.cpp \
    ../../src/EmailSettingsDialog.cpp \
    ../../src/FtpSettingsDialog.cpp \
    ../../src/Smtp/emailaddress.cpp \
    ../../src/Smtp/mimeattachment.cpp \
    ../../src/Smtp/mimecontentformatter.cpp \
    ../../src/Smtp/mimefile.cpp \
    ../../src/Smtp/mimehtml.cpp \
    ../../src/Smtp/mimeinlinefile.cpp \
    ../../src/Smtp/mimemessage.cpp \
    ../../src/Smtp/mimemultipart.cpp \
    ../../src/Smtp/mimepart.cpp \
    ../../src/Smtp/mimetext.cpp \
    ../../src/Smtp/quotedprintable.cpp \
    ../../src/Smtp/smtpclient.cpp \
    ../../src/SettingsWidget.cpp \
    ../../src/ConfigureUserDialog.cpp \
    ../../src/UsersModel.cpp \
    ../../src/UsersWidget.cpp \
    ../../src/Settings.cpp \
    ../../src/LogsWidget.cpp \
    ../../src/LogsModel.cpp \
    ../../src/Logger.cpp \
    ../../src/ExportLogsDialog.cpp \
    ../../src/PlotToolTip.cpp \
    ../../src/qftp/qftp.cpp \
    ../../src/qftp/qurlinfo.cpp

FORMS += \
    ../../src/Manager.ui \
    ../../src/BaseStationsWidget.ui \
    ../../src/MeasurementsWidget.ui \
    ../../src/SensorsWidget.ui \
    ../../src/SensorsFilterDialog.ui \
    ../../src/AlarmsWidget.ui \
    ../../src/ConfigureAlarmDialog.ui \
    ../../src/PlotWidget.ui \
    ../../src/ExportDataDialog.ui \
    ../../src/ExportPicDialog.ui \
    ../../src/ReportsWidget.ui \
    ../../src/ConfigureReportDialog.ui \
    ../../src/SensorSettingsDialog.ui \
    ../../src/EmailSettingsDialog.ui \
    ../../src/FtpSettingsDialog.ui \
    ../../src/SettingsWidget.ui \
    ../../src/ConfigureUserDialog.ui \
    ../../src/UsersWidget.ui \
    ../../src/LoginDialog.ui \
    ../../src/LogsWidget.ui \
    ../../src/ExportLogsDialog.ui

DISTFILES += \

