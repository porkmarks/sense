#pragma once

#include <QDialog>
#include "ui_EmailSettingsDialog.h"
#include "Settings.h"

class EmailSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EmailSettingsDialog(QWidget *parent = 0);
    ~EmailSettingsDialog();

    void setEmailSettings(Settings::EmailSettings const& settings);
    Settings::EmailSettings const& getEmailSettings() const;

signals:

private slots:
    void sendTestEmail();
    void accept() override;

private:
    bool getSettings(Settings::EmailSettings& settings);

    Ui::EmailSettingsDialog m_ui;
    mutable Settings::EmailSettings m_settings;
};

