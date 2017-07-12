#include "EmailSettingsDialog.h"

//////////////////////////////////////////////////////////////////////////

EmailSettingsDialog::EmailSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    m_ui.setupUi(this);
}

//////////////////////////////////////////////////////////////////////////

EmailSettingsDialog::~EmailSettingsDialog()
{
}

//////////////////////////////////////////////////////////////////////////

void EmailSettingsDialog::setEmailSettings(DB::EmailSettings const& settings)
{
    m_settings = settings;

    m_ui.host->setText(settings.host.c_str());
    m_ui.port->setValue(settings.port);
    m_ui.username->setText(settings.username.c_str());
    m_ui.password->setText(settings.password.c_str());
    m_ui.from->setText(settings.from.c_str());
}

//////////////////////////////////////////////////////////////////////////

DB::EmailSettings const& EmailSettingsDialog::getEmailSettings() const
{
    return m_settings;
}

//////////////////////////////////////////////////////////////////////////

void EmailSettingsDialog::accept()
{
    DB::EmailSettings descriptor;

    descriptor.host = m_ui.host->text().toUtf8().data();
    descriptor.port = static_cast<uint16_t>(m_ui.port->value());
    descriptor.username = m_ui.username->text().toUtf8().data();
    descriptor.password = m_ui.password->text().toUtf8().data();
    descriptor.from = m_ui.from->text().toUtf8().data();

    if (descriptor.host.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify a valid host.");
        return;
    }

    m_settings = descriptor;
    QDialog::accept();
}
