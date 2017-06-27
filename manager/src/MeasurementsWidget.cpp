#include "MeasurementsWidget.h"

MeasurementsWidget::MeasurementsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
}

void MeasurementsWidget::init(Comms& comms)
{
    m_comms = &comms;
}
