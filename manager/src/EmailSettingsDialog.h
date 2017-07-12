#pragma once

#include <QDialog>
#include "ui_EmailSettingsDialog.h"
#include "DB.h"

class EmailSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EmailSettingsDialog(QWidget *parent = 0);
    ~EmailSettingsDialog();

    void setEmailSettings(DB::EmailSettings const& settings);
    DB::EmailSettings const& getEmailSettings() const;

signals:

private slots:
    void accept() override;

private:
    Ui::EmailSettingsDialog m_ui;
    mutable DB::EmailSettings m_settings;
};

