#pragma once

#include <QDialog>
#include "ui_ExportLogsDialog.h"
#include "LogsModel.h"

#include <ostream>

class ExportLogsDialog : public QDialog
{
public:
    ExportLogsDialog(LogsModel& model, QWidget* parent);

private slots:
    void accept() override;
    void done(int result) override;

private:
    void loadSettings();
    void saveSettings();

    void refreshPreview();
    bool exportTo(std::ostream& stream, size_t maxCount, bool showProgress);

    Ui::ExportLogsDialog m_ui;
    LogsModel& m_model;
};
