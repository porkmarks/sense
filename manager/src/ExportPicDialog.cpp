#include "ExportPicDialog.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include "Logger.h"

extern Logger s_logger;


ExportPicDialog::ExportPicDialog(PlotWidget& plotWidget, QWidget* parent)
    : QDialog(parent)
    , m_plotWidget(plotWidget)
{
    m_ui.setupUi(this);

    loadSettings();
}

//////////////////////////////////////////////////////////////////////////

void ExportPicDialog::loadSettings()
{
    QSettings settings;
    m_ui.legend->setChecked(settings.value("exportPic/legend", true).toBool());
    m_ui.annotations->setChecked(settings.value("exportPic/annotations", true).toBool());
}

//////////////////////////////////////////////////////////////////////////

void ExportPicDialog::saveSettings()
{
    QSettings settings;
    settings.setValue("exportPic/legend", m_ui.legend->isChecked());
    settings.setValue("exportPic/annotations", m_ui.annotations->isChecked());
}

//////////////////////////////////////////////////////////////////////////

void ExportPicDialog::accept()
{
    saveSettings();

    QString extension = "Image (*.png)";

    QString fileName = QFileDialog::getSaveFileName(this, "Select export file", QString(), extension);
    if (fileName.isEmpty())
    {
        return;
    }

    m_plotWidget.saveToPng(fileName, m_ui.legend->isChecked(), m_ui.annotations->isChecked());

//    std::ofstream file(fileName.toUtf8().data());
//    if (!file.is_open())
//    {
//        QMessageBox::critical(this, "Error", QString("Cannot open file '%1' for writing:\n%2").arg(fileName).arg(std::strerror(errno)));
//        return;
//    }

//    exportTo(file, size_t(-1));

//    file.close();

    QString msg = QString("Plot was exported to file '%1'").arg(fileName);
    QMessageBox::information(this, "Success", msg);
    s_logger.logInfo(msg);

    QDialog::accept();
}

//////////////////////////////////////////////////////////////////////////

void ExportPicDialog::exportTo()
{
}
