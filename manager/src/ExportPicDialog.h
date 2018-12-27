#pragma once

#include <QDialog>
#include "ui_ExportPicDialog.h"
#include "PlotWidget.h"

#include <ostream>

class ExportPicDialog : public QDialog
{
public:
    ExportPicDialog(PlotWidget& plotWidget, QWidget* parent);

private slots:
    void accept() override;
    void done(int result) override;

private:
    void loadSettings();
    void saveSettings();

    void showPreview();
    void exportTo();

    Ui::ExportPicDialog m_ui;
    PlotWidget& m_plotWidget;
};
