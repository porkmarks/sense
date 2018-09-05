#pragma once

#include <QDialog>
#include "ui_SensorsConfigDialog.h"
#include "DB.h"

class SensorsConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SensorsConfigDialog(QWidget *parent = 0);
    ~SensorsConfigDialog();

    void setSensorsConfig(DB::SensorsConfig const& config);
    DB::SensorsConfig const& getSensorsConfig() const;

signals:

private slots:
    void accept() override;

private:
    Ui::SensorsConfigDialog m_ui;
    mutable DB::SensorsConfig m_config;
};

