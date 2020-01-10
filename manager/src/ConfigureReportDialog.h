#pragma once

#include <QDialog>
#include <QSortFilterProxyModel>
#include "ui_ConfigureReportDialog.h"

#include "DB.h"
#include "SensorsModel.h"
#include "SensorsDelegate.h"

class ConfigureReportDialog : public QDialog
{
public:
    ConfigureReportDialog(DB& db, QWidget* parent);

    DB::Report const& getReport() const;
    void setReport(DB::Report const& report);

private slots:
    void sendReportNow();
    void accept() override;
    void done(int result) override;

private:
    void loadSettings();
    void saveSettings();

    bool getDescriptor(DB::ReportDescriptor& descriptor);

    Ui::ConfigureReportDialog m_ui;

    DB& m_db;
    DB::Report m_report;
};
