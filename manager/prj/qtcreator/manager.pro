#-------------------------------------------------
#
# Project created by QtCreator 2014-06-04T18:33:20
#
#-------------------------------------------------

QT       += core gui opengl network widgets printsupport

TARGET = manager
TEMPLATE = app

#TARGET = manager
#target.path = /home/jean/fpv/manager
#INSTALLS = target

CONFIG += console
CONFIG += c++2a
CONFIG -= console

INCLUDEPATH += ../../src
INCLUDEPATH += ../../src/Smtp
INCLUDEPATH += ../../src/qftp
INCLUDEPATH += ../../src/sqlite
INCLUDEPATH += ../../../common/src
INCLUDEPATH += ../../../sensor/firmware


#QMAKE_CXXFLAGS += -Wno-unused-variable -Wno-unused-parameter
#QMAKE_CFLAGS += -Wno-unused-variable -Wno-unused-parameter
#QMAKE_LFLAGS += -Wl,-subsystem,windows

PRECOMPILED_HEADER = ../../src/stdafx.h
CONFIG *= precompile_header

win32: RC_ICONS = ../../res/icons/ui/manager.ico

ROOT_LIBS_PATH = ../../../..

CONFIG(debug, debug|release) {
    DEST_FOLDER = pc/debug
}
CONFIG(release, debug|release) {
    DEST_FOLDER = pc/release
    DEFINES += NDEBUG
}

#DEFINES += QCUSTOMPLOT_USE_OPENGL

win32-msvc* {
    QMAKE_LFLAGS_RELEASE += /MAP
    QMAKE_CFLAGS_RELEASE += /Zi
    QMAKE_LFLAGS_RELEASE += /debug /opt:ref
    LIBS += User32.lib
}

OBJECTS_DIR = ./.obj/$${DEST_FOLDER}
MOC_DIR = ./.moc/$${DEST_FOLDER}
RCC_DIR = ./.rcc/$${DEST_FOLDER}
UI_DIR = ./.ui/$${DEST_FOLDER}
DESTDIR = ../../bin

RESOURCES += \
    ../../res/res.qrc

FORMS += \
    ../../src/AboutDialog.ui \
    ../../src/AlarmsWidget.ui \
    ../../src/BaseStationsDialog.ui \
    ../../src/BaseStationsWidget.ui \
    ../../src/ConfigureAlarmDialog.ui \
    ../../src/ConfigureReportDialog.ui \
    ../../src/ConfigureUserDialog.ui \
    ../../src/CsvSettingsWidget.ui \
    ../../src/DateTimeFilterWidget.ui \
    ../../src/ExportDataDialog.ui \
    ../../src/ExportLogsDialog.ui \
    ../../src/ExportPicDialog.ui \
    ../../src/LoginDialog.ui \
    ../../src/LogsWidget.ui \
    ../../src/Manager.ui \
    ../../src/MeasurementDetailsDialog.ui \
    ../../src/MeasurementsWidget.ui \
    ../../src/PlotWidget.ui \
    ../../src/ReportsDialog.ui \
    ../../src/ReportsWidget.ui \
    ../../src/SensorDetailsDialog.ui \
    ../../src/SensorsFilterDialog.ui \
    ../../src/SensorsFilterWidget.ui \
    ../../src/SensorsWidget.ui \
    ../../src/SettingsDialog.ui \
    ../../src/SettingsWidget.ui \
    ../../src/UsersDialog.ui \
    ../../src/UsersWidget.ui

HEADERS += \
    ../../../common/src/Butterworth.h \
    ../../../common/src/Channel.h \
    ../../../common/src/Crypt.h \
    ../../../common/src/QTcpSocketAdapter.h \
    ../../../common/src/Queue.h \
    ../../../common/src/Result.h \
    ../../src/AlarmsModel.h \
    ../../src/AlarmsWidget.h \
    ../../src/BaseStationsWidget.h \
    ../../src/Comms.h \
    ../../src/ConfigureAlarmDialog.h \
    ../../src/ConfigureReportDialog.h \
    ../../src/ConfigureUserDialog.h \
    ../../src/CsvSettingsWidget.h \
    ../../src/DB.h \
    ../../src/DateTimeFilterWidget.h \
    ../../src/Emailer.h \
    ../../src/ExportDataDialog.h \
    ../../src/ExportLogsDialog.h \
    ../../src/ExportPicDialog.h \
    ../../src/HtmlItemDelegate.h \
    ../../src/Logger.h \
    ../../src/LogsModel.h \
    ../../src/LogsWidget.h \
    ../../src/Manager.h \
    ../../src/MeasurementDetailsDialog.h \
    ../../src/MeasurementsDelegate.h \
    ../../src/MeasurementsModel.h \
    ../../src/MeasurementsWidget.h \
    ../../src/PermissionsCheck.h \
    ../../src/PlotToolTip.h \
    ../../src/PlotWidget.h \
    ../../src/ReportsModel.h \
    ../../src/ReportsWidget.h \
    ../../src/SensorDetailsDialog.h \
    ../../src/SensorsDelegate.h \
    ../../src/SensorsFilterWidget.h \
    ../../src/SensorsModel.h \
    ../../src/SensorsWidget.h \
    ../../src/SettingsWidget.h \
    ../../src/Smtp/SmtpMime \
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
    ../../src/StackWalker.h \
    ../../src/TreeView.h \
    ../../src/UsersModel.h \
    ../../src/UsersWidget.h \
    ../../src/Utils.h \
    ../../src/qcustomplot.h \
    ../../src/qftp/qftp.h \
    ../../src/qftp/qurlinfo.h \
    ../../src/sqlite/sqlite3.h \
    ../../src/sqlite/sqlite3ext.h

SOURCES += \
    ../../../common/src/Crypt.cpp \
    ../../src/AlarmsModel.cpp \
    ../../src/AlarmsWidget.cpp \
    ../../src/BaseStationsWidget.cpp \
    ../../src/Comms.cpp \
    ../../src/ConfigureAlarmDialog.cpp \
    ../../src/ConfigureReportDialog.cpp \
    ../../src/ConfigureUserDialog.cpp \
    ../../src/CsvSettingsWidget.cpp \
    ../../src/DB.cpp \
    ../../src/DateTimeFilterWidget.cpp \
    ../../src/Emailer.cpp \
    ../../src/ExportDataDialog.cpp \
    ../../src/ExportLogsDialog.cpp \
    ../../src/ExportPicDialog.cpp \
    ../../src/HtmlItemDelegate.cpp \
    ../../src/Logger.cpp \
    ../../src/LogsModel.cpp \
    ../../src/LogsWidget.cpp \
    ../../src/Manager.cpp \
    ../../src/MeasurementDetailsDialog.cpp \
    ../../src/MeasurementsDelegate.cpp \
    ../../src/MeasurementsModel.cpp \
    ../../src/MeasurementsWidget.cpp \
    ../../src/PermissionsCheck.cpp \
    ../../src/PlotToolTip.cpp \
    ../../src/PlotWidget.cpp \
    ../../src/ReportsModel.cpp \
    ../../src/ReportsWidget.cpp \
    ../../src/SensorDetailsDialog.cpp \
    ../../src/SensorsDelegate.cpp \
    ../../src/SensorsFilterWidget.cpp \
    ../../src/SensorsModel.cpp \
    ../../src/SensorsWidget.cpp \
    ../../src/SettingsWidget.cpp \
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
    ../../src/StackWalker.cpp \
    ../../src/UsersModel.cpp \
    ../../src/UsersWidget.cpp \
    ../../src/Utils.cpp \
    ../../src/main.cpp \
    ../../src/qcustomplot.cpp \
    ../../src/qftp/qftp.cpp \
    ../../src/qftp/qurlinfo.cpp \
    ../../src/sqlite/sqlite3.c


