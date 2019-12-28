#include "PermissionsCheck.h"
#include "Settings.h"
#include "Logger.h"
#include "Crypt.h"
#include "ui_LoginDialog.h"

#include <QDialog>
#include <QMessageBox>

extern Logger s_logger;
extern std::string k_passwordHashReferenceText;

bool adminCheck(Settings& settings, QWidget* parent)
{
    Settings::User const* user = settings.getLoggedInUser();
    if (user && user->descriptor.type == Settings::UserDescriptor::Type::Admin)
    {
        return true;
    }

    s_logger.logInfo("Asking the user to log in");

    QDialog dialog(parent);
    Ui::LoginDialog ui;
    ui.setupUi(&dialog);
    ui.info->setText("The operation you're trying to do needs a user with more permissions or an admin.\nPlease login again to proceed, or press cancel to go back.");
    dialog.setWindowTitle("Login");
    dialog.adjustSize();

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        int32_t _userIndex = settings.findUserIndexByName(ui.username->text().toUtf8().data());
        if (_userIndex < 0)
        {
            s_logger.logCritical(QString("Invalid login credentials (user '%1' not found)").arg(ui.username->text()).toUtf8().data());
            QMessageBox::critical(&dialog, "Error", QString("Invalid username '%1'.").arg(ui.username->text()));
            return false;
        }

        size_t userIndex = static_cast<size_t>(_userIndex);
        Settings::User const& user = settings.getUser(userIndex);
        if (user.descriptor.type != Settings::UserDescriptor::Type::Admin)
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

bool hasPermission(Settings& settings, Settings::UserDescriptor::Permissions permission)
{
	Settings::User const* user = settings.getLoggedInUser();
    if (!user)
    {
        return false;
    }

	if (user->descriptor.type == Settings::UserDescriptor::Type::Admin)
	{
		return true;
	}
    return (user->descriptor.permissions & permission) != 0;
}

bool hasPermissionOrCanLoginAsAdmin(Settings& settings, Settings::UserDescriptor::Permissions permission, QWidget* parent)
{
    return hasPermission(settings, permission) || adminCheck(settings, parent);
}