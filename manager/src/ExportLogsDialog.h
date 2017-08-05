#pragma once

#include <QDialog>
#include "ui_ExportLogsDialog.h"
#include "LogsModel.h"

#include <ostream>

class ExportLogsDialog : public QDialog
{
public:
    ExportLogsDialog(LogsModel& model);

private slots:
    void accept() override;

private:

    void refreshPreview();
    void exportTo(std::ostream& stream, size_t maxCount);

    Ui::ExportLogsDialog m_ui;
    LogsModel& m_model;
};
