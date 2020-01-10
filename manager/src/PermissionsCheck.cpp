#include "PermissionsCheck.h"
#include "DB.h"
#include "Logger.h"
#include "Crypt.h"
#include "ui_LoginDialog.h"

#include <QDialog>
#include <QMessageBox>

extern Logger s_logger;
extern std::string k_passwordHashReferenceText;

bool adminCheck(DB& db, QWidget* parent)
{
	DB::User const* user = db.getLoggedInUser();
	if (user && user->descriptor.type == DB::UserDescriptor::Type::Admin)
    {
        return true;
    }

    s_logger.logInfo("Asking the user to log in");

    QDialog dialog(parent);
    Ui::LoginDialog ui;
    ui.setupUi(&dialog);
    ui.info->setText("The operation you're trying to do needs a user with more permissions or an admin.\nNOTE: you will not be logged out after this. This test is for the current operation only.");
    dialog.setWindowTitle("Admin Credentials Needed");
    dialog.adjustSize();

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
		int32_t _userIndex = db.findUserIndexByName(ui.username->text().toUtf8().data());
        if (_userIndex < 0)
        {
            s_logger.logCritical(QString("Invalid login credentials (user '%1' not found)").arg(ui.username->text()).toUtf8().data());
            QMessageBox::critical(&dialog, "Error", QString("Invalid username '%1'.").arg(ui.username->text()));
            return false;
        }

        size_t userIndex = static_cast<size_t>(_userIndex);
		DB::User const& user = db.getUser(userIndex);
		if (user.descriptor.type != DB::UserDescriptor::Type::Admin)
        {
            s_logger.logCritical(QString("Invalid login credentials (user '%1' not admin)").arg(ui.username->text()).toUtf8().data());
            QMessageBox::critical(&dialog, "Error", QString("Invalid user '%1': not admin.").arg(ui.username->text()));
            return false;
        }

        Crypt crypt;
        crypt.setAddRandomSalt(false);
        crypt.setIntegrityProtectionMode(Crypt::ProtectionHash);
        crypt.setCompressionMode(Crypt::CompressionAlways);
        crypt.setKey(ui.password->text());
        std::string passwordHash = crypt.encryptToString(QString(k_passwordHashReferenceText.c_str())).toUtf8().data();
        if (user.descriptor.passwordHash != passwordHash)
        {
            s_logger.logCritical(QString("Invalid login credentials (wrong password)").toUtf8().data());
            QMessageBox::critical(&dialog, "Error", "Invalid username/password.");
            return false;
        }

        return true;
    }
    s_logger.logCritical("User adming check cancelled");
    return false;
}

bool hasPermission(DB& db, DB::UserDescriptor::Permissions permission)
{
	DB::User const* user = db.getLoggedInUser();
    if (!user)
    {
        return false;
    }

	if (user->descriptor.type == DB::UserDescriptor::Type::Admin)
	{
		return true;
	}
    return (user->descriptor.permissions & permission) != 0;
}

bool hasPermissionOrCanLoginAsAdmin(DB& db, DB::UserDescriptor::Permissions permission, QWidget* parent)
{
	return hasPermission(db, permission) || adminCheck(db, parent);
}