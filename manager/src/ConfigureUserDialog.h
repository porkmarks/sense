#pragma once

#include <QDialog>
#include "ui_ConfigureUserDialog.h"

#include "Settings.h"

class ConfigureUserDialog : public QDialog
{
public:
    ConfigureUserDialog(Settings& settings, QWidget* parent);

    Settings::User const& getUser() const;
    void setUser(Settings::User const& user);

    void setForcedType(Settings::UserDescriptor::Type type);

private slots:
    void accept() override;

private:
    Ui::ConfigureUserDialog m_ui;

    Settings& m_settings;
    Settings::User m_user;
};
