#pragma once

#include <QDialog>
#include "ui_SensorSettingsDialog.h"
#include "DB.h"

class SensorSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SensorSettingsDialog(QWidget *parent = 0);
    ~SensorSettingsDialog();

    void setSensorSettings(DB::SensorSettings const& settings);
    DB::SensorSettings const& getSensorSettings() const;

signals:

private slots:
    void accept() override;

private:
    Ui::SensorSettingsDialog m_ui;
    mutable DB::SensorSettings m_settings;
};

