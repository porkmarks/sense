#pragma once

#include <QDialog>
#include "ui_ExportPicDialog.h"
#include "PlotWidget.h"

#include <ostream>

class ExportPicDialog : public QDialog
{
public:
    ExportPicDialog(PlotWidget& plotWidget);

private slots:
    void accept() override;

private:

    void showPreview();
    void exportTo();

    Ui::ExportPicDialog m_ui;
    PlotWidget& m_plotWidget;
};
