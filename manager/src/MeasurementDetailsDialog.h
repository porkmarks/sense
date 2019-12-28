#pragma once

#include <QDialog>
#include <QSortFilterProxyModel>
#include "ui_MeasurementDetailsDialog.h"

#include "DB.h"

class MeasurementDetailsDialog : public QDialog
{
public:
    MeasurementDetailsDialog(DB& db, QWidget* parent);

    DB::Measurement const& getMeasurement() const;
    void setMeasurement(DB::Measurement const& measurement);

private slots:
    void accept() override;
    void done(int result) override;

private:
    void loadSettings();
    void saveSettings();
    void closeEvent(QCloseEvent* event) override;

    Ui::MeasurementDetailsDialog m_ui;

    DB& m_db;
    DB::Measurement m_measurement;
};
