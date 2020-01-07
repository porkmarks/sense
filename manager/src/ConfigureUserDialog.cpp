#include "ConfigureUserDialog.h"
#include "Crypt.h"
#include <QMessageBox>
#include <QSettings>
#include "PermissionsCheck.h"

std::string k_passwordHashReferenceText = "Cannot open file.";

//////////////////////////////////////////////////////////////////////////

ConfigureUserDialog::ConfigureUserDialog(Settings& settings, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
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

Settings::User const& ConfigureUserDialog::getUser() const
{
    return m_user;
}

//////////////////////////////////////////////////////////////////////////

void ConfigureUserDialog::setUser(Settings::User const& user)
{
    m_user = user;
    Settings::UserDescriptor descriptor = m_user.descriptor;

    m_ui.name->setText(descriptor.name.c_str());

    if (m_ui.administrator->isEnabled())
	{
		m_ui.administrator->setChecked(descriptor.type == Settings::UserDescriptor::Type::Admin);
	}
    if (descriptor.type != Settings::UserDescriptor::Type::Admin)
    {
        m_ui.addRemoveSensors->setChecked(descriptor.permissions & Settings::UserDescriptor::PermissionAddRemoveSensors);
        m_ui.changeSensors->setChecked(descriptor.permissions & Settings::UserDescriptor::PermissionChangeSensors);

		m_ui.addRemoveBaseStations->setChecked(descriptor.permissions & Settings::UserDescriptor::PermissionAddRemoveBaseStations);
		m_ui.changeBaseStations->setChecked(descriptor.permissions & Settings::UserDescriptor::PermissionChangeBaseStations);

		m_ui.addRemoveAlarms->setChecked(descriptor.permissions & Settings::UserDescriptor::PermissionAddRemoveAlarms);
		m_ui.changeAlarms->setChecked(descriptor.permissions & Settings::UserDescriptor::PermissionChangeAlarms);

		m_ui.addRemoveReports->setChecked(descriptor.permissions & Settings::UserDescriptor::PermissionAddRemoveReports);
		m_ui.changeReports->setChecked(descriptor.permissions & Settings::UserDescriptor::PermissionChangeReports);

		m_ui.addRemoveUsers->setChecked(descriptor.permissions & Settings::UserDescriptor::PermissionAddRemoveUsers);
		m_ui.changeUsers->setChecked(descriptor.permissions & Settings::UserDescriptor::PermissionChangeUsers);

        m_ui.changeMeasurements->setChecked(descriptor.permissions & Settings::UserDescriptor::PermissionChangeMeasurements);
		m_ui.changeEmailSettings->setChecked(descriptor.permissions & Settings::UserDescriptor::PermissionChangeEmailSettings);
		m_ui.changeFtpSettings->setChecked(descriptor.permissions & Settings::UserDescriptor::PermissionChangeFtpSettings);
		m_ui.changeSensorSettings->setChecked(descriptor.permissions & Settings::UserDescriptor::PermissionChangeSensorSettings);
    }

    if (!hasPermission(m_settings, Settings::UserDescriptor::PermissionChangeUsers))
    {
        m_ui.administrator->setEnabled(false);
        m_ui.permissions->setEnabled(false);
    }

    m_ui.password1->clear();
    m_ui.password2->clear();
}

//////////////////////////////////////////////////////////////////////////

void ConfigureUserDialog::setForcedType(Settings::UserDescriptor::Type type)
{
    m_ui.administrator->setChecked(type == Settings::UserDescriptor::Type::Admin);
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
    Settings::UserDescriptor descriptor = m_user.descriptor;
    descriptor.name = m_ui.name->text().toUtf8().data();
    if (descriptor.name.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify a name for this user.");
        return;
    }

	descriptor.type = m_ui.administrator->isChecked() ? Settings::UserDescriptor::Type::Admin : Settings::UserDescriptor::Type::Normal;
    descriptor.permissions = 0;
	descriptor.permissions |= m_ui.addRemoveSensors->isChecked() ? Settings::UserDescriptor::PermissionAddRemoveSensors : 0;
	descriptor.permissions |= m_ui.changeSensors->isChecked() ? Settings::UserDescriptor::PermissionChangeSensors : 0;
	descriptor.permissions |= m_ui.addRemoveBaseStations->isChecked() ? Settings::UserDescriptor::PermissionAddRemoveBaseStations : 0;
	descriptor.permissions |= m_ui.changeBaseStations->isChecked() ? Settings::UserDescriptor::PermissionChangeBaseStations : 0;
	descriptor.permissions |= m_ui.addRemoveAlarms->isChecked() ? Settings::UserDescriptor::PermissionAddRemoveAlarms : 0;
	descriptor.permissions |= m_ui.changeAlarms->isChecked() ? Settings::UserDescriptor::PermissionChangeAlarms : 0;
	descriptor.permissions |= m_ui.addRemoveReports->isChecked() ? Settings::UserDescriptor::PermissionAddRemoveReports : 0;
	descriptor.permissions |= m_ui.changeReports->isChecked() ? Settings::UserDescriptor::PermissionChangeReports : 0;
	descriptor.permissions |= m_ui.addRemoveUsers->isChecked() ? Settings::UserDescriptor::PermissionAddRemoveUsers : 0;
	descriptor.permissions |= m_ui.changeUsers->isChecked() ? Settings::UserDescriptor::PermissionChangeUsers : 0;
	descriptor.permissions |= m_ui.changeMeasurements->isChecked() ? Settings::UserDescriptor::PermissionChangeMeasurements : 0;
	descriptor.permissions |= m_ui.changeEmailSettings->isChecked() ? Settings::UserDescriptor::PermissionChangeEmailSettings : 0;
	descriptor.permissions |= m_ui.changeFtpSettings->isChecked() ? Settings::UserDescriptor::PermissionChangeFtpSettings : 0;
	descriptor.permissions |= m_ui.changeSensorSettings->isChecked() ? Settings::UserDescriptor::PermissionChangeSensorSettings : 0;

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

    int32_t userIndex = m_settings.findUserIndexByName(descriptor.name);
    if (userIndex >= 0 && m_settings.getUser(static_cast<size_t>(userIndex)).id != m_user.id)
    {
        QMessageBox::critical(this, "Error", QString("User '%1' already exists.").arg(descriptor.name.c_str()));
        return;
    }

    m_user.descriptor = descriptor;
    QDialog::accept();
}

//////////////////////////////////////////////////////////////////////////

