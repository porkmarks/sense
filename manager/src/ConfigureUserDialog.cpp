#include "ConfigureUserDialog.h"
#include "Crypt.h"
#include <QMessageBox>
#include <QSettings>
#include "PermissionsCheck.h"

std::string k_passwordHashReferenceText = "Cannot open file.";

//////////////////////////////////////////////////////////////////////////

ConfigureUserDialog::ConfigureUserDialog(DB& db, QWidget* parent)
    : QDialog(parent)
    , m_db(db)
{
    m_ui.setupUi(this);

    adjustSize();
    loadSettings();
}

//////////////////////////////////////////////////////////////////////////

void ConfigureUserDialog::loadSettings()
{
    QSettings settings;
    resize(settings.value("ConfigureUserDialog/size", size()).toSize());
    QPoint defaultPos = parentWidget()->mapToGlobal(parentWidget()->rect().center()) - QPoint(width() / 2, height() / 2);
    move(settings.value("ConfigureUserDialog/pos", defaultPos).toPoint());
}

//////////////////////////////////////////////////////////////////////////

void ConfigureUserDialog::saveSettings()
{
    QSettings settings;
    settings.setValue("ConfigureUserDialog/size", size());
    settings.setValue("ConfigureUserDialog/pos", pos());
}

//////////////////////////////////////////////////////////////////////////

DB::User const& ConfigureUserDialog::getUser() const
{
    return m_user;
}

//////////////////////////////////////////////////////////////////////////

void ConfigureUserDialog::setUser(DB::User const& user)
{
    m_user = user;
    DB::UserDescriptor descriptor = m_user.descriptor;

    m_ui.name->setText(descriptor.name.c_str());

    if (m_ui.administrator->isEnabled())
	{
		m_ui.administrator->setChecked(descriptor.type == DB::UserDescriptor::Type::Admin);
	}
    if (descriptor.type != DB::UserDescriptor::Type::Admin)
    {
        m_ui.addRemoveSensors->setChecked(descriptor.permissions & DB::UserDescriptor::PermissionAddRemoveSensors);
        m_ui.changeSensors->setChecked(descriptor.permissions & DB::UserDescriptor::PermissionChangeSensors);

		m_ui.addRemoveBaseStations->setChecked(descriptor.permissions & DB::UserDescriptor::PermissionAddRemoveBaseStations);
		m_ui.changeBaseStations->setChecked(descriptor.permissions & DB::UserDescriptor::PermissionChangeBaseStations);

		m_ui.addRemoveAlarms->setChecked(descriptor.permissions & DB::UserDescriptor::PermissionAddRemoveAlarms);
		m_ui.changeAlarms->setChecked(descriptor.permissions & DB::UserDescriptor::PermissionChangeAlarms);

		m_ui.addRemoveReports->setChecked(descriptor.permissions & DB::UserDescriptor::PermissionAddRemoveReports);
		m_ui.changeReports->setChecked(descriptor.permissions & DB::UserDescriptor::PermissionChangeReports);

		m_ui.addRemoveUsers->setChecked(descriptor.permissions & DB::UserDescriptor::PermissionAddRemoveUsers);
		m_ui.changeUsers->setChecked(descriptor.permissions & DB::UserDescriptor::PermissionChangeUsers);

        m_ui.changeMeasurements->setChecked(descriptor.permissions & DB::UserDescriptor::PermissionChangeMeasurements);
		m_ui.changeEmailSettings->setChecked(descriptor.permissions & DB::UserDescriptor::PermissionChangeEmailSettings);
		m_ui.changeFtpSettings->setChecked(descriptor.permissions & DB::UserDescriptor::PermissionChangeFtpSettings);
		m_ui.changeSensorSettings->setChecked(descriptor.permissions & DB::UserDescriptor::PermissionChangeSensorSettings);
    }

	if (!hasPermission(m_db, DB::UserDescriptor::PermissionChangeUsers))
    {
        m_ui.administrator->setEnabled(false);
        m_ui.permissions->setEnabled(false);
    }

    m_ui.password1->clear();
    m_ui.password2->clear();
}

//////////////////////////////////////////////////////////////////////////

void ConfigureUserDialog::setForcedType(DB::UserDescriptor::Type type)
{
    m_ui.administrator->setChecked(type == DB::UserDescriptor::Type::Admin);
    m_ui.administrator->setEnabled(false);
}

//////////////////////////////////////////////////////////////////////////

void ConfigureUserDialog::done(int result)
{
    saveSettings();
    QDialog::done(result);
}

//////////////////////////////////////////////////////////////////////////

void ConfigureUserDialog::accept()
{
    DB::UserDescriptor descriptor = m_user.descriptor;
    descriptor.name = m_ui.name->text().toUtf8().data();
    if (descriptor.name.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify a name for this user.");
        return;
    }

	descriptor.type = m_ui.administrator->isChecked() ? DB::UserDescriptor::Type::Admin : DB::UserDescriptor::Type::Normal;
    descriptor.permissions = 0;
	descriptor.permissions |= m_ui.addRemoveSensors->isChecked() ? DB::UserDescriptor::PermissionAddRemoveSensors : 0;
	descriptor.permissions |= m_ui.changeSensors->isChecked() ? DB::UserDescriptor::PermissionChangeSensors : 0;
	descriptor.permissions |= m_ui.addRemoveBaseStations->isChecked() ? DB::UserDescriptor::PermissionAddRemoveBaseStations : 0;
	descriptor.permissions |= m_ui.changeBaseStations->isChecked() ? DB::UserDescriptor::PermissionChangeBaseStations : 0;
	descriptor.permissions |= m_ui.addRemoveAlarms->isChecked() ? DB::UserDescriptor::PermissionAddRemoveAlarms : 0;
	descriptor.permissions |= m_ui.changeAlarms->isChecked() ? DB::UserDescriptor::PermissionChangeAlarms : 0;
	descriptor.permissions |= m_ui.addRemoveReports->isChecked() ? DB::UserDescriptor::PermissionAddRemoveReports : 0;
	descriptor.permissions |= m_ui.changeReports->isChecked() ? DB::UserDescriptor::PermissionChangeReports : 0;
	descriptor.permissions |= m_ui.addRemoveUsers->isChecked() ? DB::UserDescriptor::PermissionAddRemoveUsers : 0;
	descriptor.permissions |= m_ui.changeUsers->isChecked() ? DB::UserDescriptor::PermissionChangeUsers : 0;
	descriptor.permissions |= m_ui.changeMeasurements->isChecked() ? DB::UserDescriptor::PermissionChangeMeasurements : 0;
	descriptor.permissions |= m_ui.changeEmailSettings->isChecked() ? DB::UserDescriptor::PermissionChangeEmailSettings : 0;
	descriptor.permissions |= m_ui.changeFtpSettings->isChecked() ? DB::UserDescriptor::PermissionChangeFtpSettings : 0;
	descriptor.permissions |= m_ui.changeSensorSettings->isChecked() ? DB::UserDescriptor::PermissionChangeSensorSettings : 0;

    if (m_ui.password1->text().isEmpty() && m_ui.password2->text().isEmpty())
    {
        if (descriptor.passwordHash.empty())
        {
            QMessageBox::critical(this, "Error", "Please enter a password.");
            return;
        }
    }
    else
    {
        if (m_ui.password1->text() != m_ui.password2->text())
        {
            QMessageBox::critical(this, "Error", "The passwords don't match.");
            return;
        }

        Crypt crypt;
        crypt.setAddRandomSalt(false);
        crypt.setIntegrityProtectionMode(Crypt::ProtectionHash);
        crypt.setCompressionMode(Crypt::CompressionAlways);
        crypt.setKey(m_ui.password1->text());
        descriptor.passwordHash = crypt.encryptToString(QString(k_passwordHashReferenceText.c_str())).toUtf8().data();
    }

    std::optional<DB::User> user = m_db.findUserByName(descriptor.name);
    if (user.has_value() && user->id != m_user.id)
    {
        QMessageBox::critical(this, "Error", QString("User '%1' already exists.").arg(descriptor.name.c_str()));
        return;
    }

    m_user.descriptor = descriptor;
    QDialog::accept();
}

//////////////////////////////////////////////////////////////////////////

