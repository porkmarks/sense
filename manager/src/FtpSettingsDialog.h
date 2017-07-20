#pragma once

#include <QDialog>
#include "ui_FtpSettingsDialog.h"
#include "Settings.h"

class FtpSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FtpSettingsDialog(QWidget *parent = 0);
    ~FtpSettingsDialog();

    void setFtpSettings(Settings::FtpSettings const& settings);
    Settings::FtpSettings const& getFtpSettings() const;

signals:

private slots:
    void accept() override;

private:
    Ui::FtpSettingsDialog m_ui;
    mutable Settings::FtpSettings m_settings;
};

