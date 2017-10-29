#pragma once

#include <QWidget>
#include "ui_SettingsWidget.h"
#include "Comms.h"
#include "Settings.h"
#include "DB.h"

#include "SensorsModel.h"
#include "SensorsDelegate.h"
#include <QSortFilterProxyModel>

class SettingsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsWidget(QWidget *parent = 0);
    ~SettingsWidget();

    void init(Comms& comms, Settings& settings);
    void shutdown();

    void initBaseStation(Settings::BaseStationId id);
    void shutdownBaseStation(Settings::BaseStationId id);

signals:

private slots:
    void setRW();
    void sendTestEmail();
    void testFtpSettings();
    void applyEmailSettings();
    void applyFtpSettings();
    void applySensorSettings();
    void addEmailRecipient();
    void removeEmailRecipient();

private:
    void setEmailSettings(Settings::EmailSettings const& settings);
    bool getEmailSettings(Settings::EmailSettings& settings);

    void setFtpSettings(Settings::FtpSettings const& settings);
    bool getFtpSettings(Settings::FtpSettings& settings);

    void setSensorSettings(DB::SensorSettings const& settings);
    bool getSensorSettings(DB::SensorSettingsDescriptor& settings);

    Ui::SettingsWidget m_ui;
    Settings* m_settings = nullptr;
    DB* m_db = nullptr;
    std::vector<QMetaObject::Connection> m_uiConnections;
};

