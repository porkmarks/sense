#pragma once

#include <QDialog>
#include "ui_ConfigureUserDialog.h"

#include "DB.h"

class ConfigureUserDialog : public QDialog
{
public:
	ConfigureUserDialog(DB& db, QWidget* parent);

	DB::User const& getUser() const;
    void setUser(DB::User const& user);

    void setForcedType(DB::UserDescriptor::Type type);

private slots:
    void accept() override;
    void done(int result) override;

private:
    void loadSettings();
    void saveSettings();

    Ui::ConfigureUserDialog m_ui;

	DB& m_db;
	DB::User m_user;
};
