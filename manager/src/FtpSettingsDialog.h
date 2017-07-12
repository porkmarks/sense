#pragma once

#include <QDialog>
#include "ui_FtpSettingsDialog.h"
#include "DB.h"

class FtpSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FtpSettingsDialog(QWidget *parent = 0);
    ~FtpSettingsDialog();

    void setFtpSettings(DB::FtpSettings const& settings);
    DB::FtpSettings const& getFtpSettings() const;

signals:

private slots:
    void accept() override;

private:
    Ui::FtpSettingsDialog m_ui;
    mutable DB::FtpSettings m_settings;
};

