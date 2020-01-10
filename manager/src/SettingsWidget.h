#pragma once

#include <QWidget>
#include "ui_SettingsWidget.h"
#include "Comms.h"
#include "DB.h"
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

	void init(Comms& comms, DB& db);
    void shutdown();
    void save();

signals:

private slots:
    void setPermissions();
    void sendTestEmail();
    void testFtpSettings();
    void applyEmailSettings();
    void applyFtpSettings();
    void applySensorTimeConfig();
    void applySensorSettings();
    void addEmailRecipient();
    void removeEmailRecipient();
    void emailSmtpProviderPresetChanged();
    void resetEmailProviderPreset();

private:
	void setEmailSettings(DB::EmailSettings const& settings);
    bool getEmailSettings(DB::EmailSettings& settings);

    void setFtpSettings(DB::FtpSettings const& settings);
    bool getFtpSettings(DB::FtpSettings& settings);

    void setSensorTimeConfig(DB::SensorTimeConfig const& config);
    Result<DB::SensorTimeConfigDescriptor> getSensorTimeConfig() const;

	void setSensorSettings(DB::SensorSettings const& config);
	Result<DB::SensorSettings> getSensorSettings() const;

    void computeBatteryLife();

    Ui::SettingsWidget m_ui;
    DB* m_db = nullptr;
    std::vector<QMetaObject::Connection> m_uiConnections;
    std::vector<QMetaObject::Connection> m_dbConnections;
};

