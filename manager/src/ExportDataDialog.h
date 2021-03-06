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

    Ui::ExportDataDialog m_ui;
    DB& m_db;
    MeasurementsModel& m_model;
};
