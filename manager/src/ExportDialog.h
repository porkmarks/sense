#pragma once

#include <QDialog>
#include "ui_ExportDialog.h"
#include "MeasurementsModel.h"

#include <ostream>

class ExportDialog : public QDialog
{
public:
    ExportDialog(DB& dm, MeasurementsModel& model);

private slots:
    void accept() override;

private:

    void showPreview();
    void exportTo(std::ostream& stream, size_t maxCount);

    Ui::ExportDialog m_ui;
    DB& m_db;
    MeasurementsModel& m_model;
};
