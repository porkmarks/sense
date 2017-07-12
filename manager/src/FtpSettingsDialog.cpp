#include "FtpSettingsDialog.h"

//////////////////////////////////////////////////////////////////////////

FtpSettingsDialog::FtpSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    m_ui.setupUi(this);
}

//////////////////////////////////////////////////////////////////////////

FtpSettingsDialog::~FtpSettingsDialog()
{
}

//////////////////////////////////////////////////////////////////////////

void FtpSettingsDialog::setFtpSettings(DB::FtpSettings const& settings)
{
    m_settings = settings;

    m_ui.host->setText(settings.host.c_str());
    m_ui.username->setText(settings.username.c_str());
    m_ui.password->setText(settings.password.c_str());
}

//////////////////////////////////////////////////////////////////////////

DB::FtpSettings const& FtpSettingsDialog::getFtpSettings() const
{
    return m_settings;
}

//////////////////////////////////////////////////////////////////////////

void FtpSettingsDialog::accept()
{
    DB::FtpSettings descriptor;

    descriptor.host = m_ui.host->text().toUtf8().data();
    descriptor.username = m_ui.username->text().toUtf8().data();
    descriptor.password = m_ui.password->text().toUtf8().data();

    if (descriptor.host.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify a valid host.");
        return;
    }

    m_settings = descriptor;
    QDialog::accept();
}
