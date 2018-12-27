#pragma once

#include <QDialog>
#include <QSortFilterProxyModel>
#include "ui_SensorDetailsDialog.h"

#include "DB.h"

class SensorDetailsDialog : public QDialog
{
public:
    SensorDetailsDialog(DB& db, QWidget* parent);

    DB::Sensor const& getSensor() const;
    void setSensor(DB::Sensor const& sensor);

private slots:
    void accept() override;
    void done(int result) override;

private:
    void loadSettings();
    void saveSettings();
    void closeEvent(QCloseEvent* event) override;

    Ui::SensorDetailsDialog m_ui;

    DB& m_db;
    DB::Sensor m_sensor;
};
