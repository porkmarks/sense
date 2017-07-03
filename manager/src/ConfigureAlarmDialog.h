#pragma once

#include <QDialog>
#include <QSortFilterProxyModel>
#include "ui_ConfigureAlarmDialog.h"

#include "DB.h"
#include "SensorsModel.h"
#include "SensorsDelegate.h"

class ConfigureAlarmDialog : public QDialog
{
public:
    ConfigureAlarmDialog(DB& db);

    DB::Alarm const& getAlarm() const;
    void setAlarm(DB::Alarm const& alarm);

private slots:
    void accept() override;

private:
    Ui::ConfigureAlarmDialog m_ui;

    DB& m_db;
    DB::Alarm m_alarm;
    SensorsModel m_model;
    QSortFilterProxyModel m_sortingModel;
    SensorsDelegate m_delegate;
};
