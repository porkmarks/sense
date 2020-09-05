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
    adjustSize();
    loadSettings();
}

//////////////////////////////////////////////////////////////////////////

void ExportPicDialog::loadSettings()
{
    QSettings settings;
    m_ui.legend->setChecked(settings.value("ExportPicDialog/legend", true).toBool());
    m_ui.annotations->setChecked(settings.value("ExportPicDialog/annotations", true).toBool());
    resize(settings.value("ExportPicDialog/size", size()).toSize());
    QPoint defaultPos = parentWidget()->mapToGlobal(parentWidget()->rect().center()) - QPoint(width() / 2, height() / 2);
    move(settings.value("ExportPicDialog/pos", defaultPos).toPoint());
}

//////////////////////////////////////////////////////////////////////////

void ExportPicDialog::saveSettings()
{
    QSettings settings;
    settings.setValue("ExportPicDialog/legend", m_ui.legend->isChecked());
    settings.setValue("ExportPicDialog/annotations", m_ui.annotations->isChecked());
    settings.setValue("ExportPicDialog/size", size());
    settings.setValue("ExportPicDialog/pos", pos());
}

//////////////////////////////////////////////////////////////////////////

void ExportPicDialog::done(int result)
{
    saveSettings();
    QDialog::done(result);
}

//////////////////////////////////////////////////////////////////////////

void ExportPicDialog::accept()
{
    QString extension = "Image (*.png)";

    QString fileName = QFileDialog::getSaveFileName(this, "Select export file", QString(), extension);
    if (fileName.isEmpty())
        return;

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
