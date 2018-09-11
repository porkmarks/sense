#include "ConfigureUserDialog.h"
#include "Crypt.h"
#include <QMessageBox>

std::string k_passwordHashReferenceText = "Cannot open file.";

//////////////////////////////////////////////////////////////////////////

ConfigureUserDialog::ConfigureUserDialog(Settings& settings)
    : m_settings(settings)
{
    m_ui.setupUi(this);
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

    m_ui.administrator->setChecked(descriptor.type == Settings::UserDescriptor::Type::Admin);
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

void ConfigureUserDialog::accept()
{
    Settings::UserDescriptor descriptor;
    descriptor.name = m_ui.name->text().toUtf8().data();
    if (descriptor.name.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify a name for this user.");
        return;
    }

    descriptor.type = m_ui.administrator->isChecked() ? Settings::UserDescriptor::Type::Admin : Settings::UserDescriptor::Type::Normal;

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

