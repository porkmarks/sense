#pragma once

#include <QWidget>
#include "ui_MeasurementsWidget.h"
#include "Comms.h"

class MeasurementsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MeasurementsWidget(QWidget *parent = 0);
    void init(Comms& comms);

signals:

public slots:

private:
    Ui::MeasurementsWidget m_ui;
    Comms* m_comms = nullptr;
};

