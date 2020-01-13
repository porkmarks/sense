#pragma once

#include <QDialog>
#include "ui_ExportDataDialog.h"
#include "MeasurementsModel.h"

#include <ostream>
#include "Utils.h"

class QProgressDialog;

class ExportDataDialog : public QDialog
{
public:
    ExportDataDialog(DB& dm, MeasurementsModel& model, QWidget* parent);

private slots:
    void accept() override;
    void done(int result) override;

private:
    void loadSettings();
    void saveSettings();

    void refreshPreview();
    std::optional<utils::CsvData> getCsvData(size_t index) const;
    std::optional<utils::CsvData> getCsvDataWithProgress(size_t index, QProgressDialog* progressDialog) const;

    Ui::ExportDataDialog m_ui;
    DB& m_db;
    MeasurementsModel& m_model;
};
