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
	std::optional<DB::User> user = db.getLoggedInUser();
	if (user && user->descriptor.type == DB::UserDescriptor::Type::Admin)
        return true;

    s_logger.logInfo("Asking the user to log in");

    QDialog dialog(parent);
    Ui::LoginDialog ui;
    ui.setupUi(&dialog);
    ui.info->setText("The operation you're trying to do needs an admin account.\nNOTE: you will not be logged out after this. This is for the current operation only.");
    dialog.setWindowTitle("Admin Credentials Needed");
    dialog.adjustSize();

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        std::optional<DB::User> user = db.findUserByName(ui.username->text().toUtf8().data());
        if (!user.has_value())
        {
            s_logger.logCritical(QString("Invalid login credentials (user '%1' not found)").arg(ui.username->text()).toUtf8().data());
            QMessageBox::critical(&dialog, "Error", QString("Invalid username '%1'.").arg(ui.username->text()));
            return false;
        }

        if (user->descriptor.type != DB::UserDescriptor::Type::Admin)
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
        if (user->descriptor.passwordHash != passwordHash)
        {
            s_logger.logCritical(QString("Invalid login credentials (wrong password)").toUtf8().data());
            QMessageBox::critical(&dialog, "Error", "Invalid username/password.");
            return false;
        }

        return true;
    }
    s_logger.logCritical("Admin check canceled");
    return false;
}

bool hasPermission(DB& db, DB::UserDescriptor::Permissions permission)
{
    std::optional<DB::User> user = db.getLoggedInUser();
    if (!user)
        return false;

	if (user->descriptor.type == DB::UserDescriptor::Type::Admin)
		return true;

    return (user->descriptor.permissions & permission) != 0;
}

bool hasPermissionOrCanLoginAsAdmin(DB& db, DB::UserDescriptor::Permissions permission, QWidget* parent)
{
	return hasPermission(db, permission) || adminCheck(db, parent);
}
