#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include "ui_SensorsWidget.h"
#include "DB.h"
#include "SensorsModel.h"

class SensorsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SensorsWidget(QWidget *parent = 0);
    ~SensorsWidget();
    void init(DB& dm);
    void shutdown();

signals:

private slots:
    void bindSensor();
    void unbindSensor();

private:
    Ui::SensorsWidget m_ui;
    std::unique_ptr<SensorsModel> m_model;
    DB* m_db = nullptr;
};

