#pragma once

#include <QWidget>
#include "ui_AlarmsWidget.h"
#include "DB.h"
#include "AlarmsModel.h"


class AlarmsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AlarmsWidget(QWidget *parent = 0);
    ~AlarmsWidget();
    void init(DB& db);
    void shutdown();

signals:

private slots:
    void addAlarm();
    void removeAlarms();

private:
    Ui::AlarmsWidget m_ui;
    std::unique_ptr<AlarmsModel> m_model;
    DB* m_db = nullptr;
};

