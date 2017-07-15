#pragma once

#include <QDialog>
#include "ui_ExportDataDialog.h"
#include "MeasurementsModel.h"

#include <ostream>

class ExportDataDialog : public QDialog
{
public:
    ExportDataDialog(DB& dm, MeasurementsModel& model);

private slots:
    void accept() override;

private:

    void refreshPreview();
    void exportTo(std::ostream& stream, size_t maxCount);

    Ui::ExportDataDialog m_ui;
    DB& m_db;
    MeasurementsModel& m_model;
};
