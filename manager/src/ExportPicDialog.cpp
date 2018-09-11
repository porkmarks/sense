#include "ExportPicDialog.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>

ExportPicDialog::ExportPicDialog(PlotWidget& plotWidget)
    : m_plotWidget(plotWidget)
{
    m_ui.setupUi(this);
    m_ui.width->setValue(plotWidget.getPlotSize().width());
    m_ui.height->setValue(plotWidget.getPlotSize().height());
}

//////////////////////////////////////////////////////////////////////////

void ExportPicDialog::accept()
{
    QString extension = "Image (*.png)";

    QString fileName = QFileDialog::getSaveFileName(this, "Select export file", QString(), extension);
    if (fileName.isEmpty())
    {
        return;
    }

    m_plotWidget.saveToPng(fileName, m_ui.legend->isChecked());

//    std::ofstream file(fileName.toUtf8().data());
//    if (!file.is_open())
//    {
//        QMessageBox::critical(this, "Error", QString("Cannot open file '%1' for writing:\n%2").arg(fileName).arg(std::strerror(errno)));
//        return;
//    }

//    exportTo(file, size_t(-1));

//    file.close();

    QMessageBox::information(this, "Success", QString("Image was exported to file '%1'").arg(fileName));

    QDialog::accept();
}

//////////////////////////////////////////////////////////////////////////

void ExportPicDialog::exportTo()
{
}
