#pragma once

#include <QWidget>
#include "ui_AlarmsWidget.h"
#include "DB.h"
#include "AlarmsModel.h"

#include "SensorsModel.h"
#include "SensorsDelegate.h"
#include <QSortFilterProxyModel>

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
    void configureAlarm(QModelIndex const& index);

private:

    Ui::AlarmsWidget m_ui;
    std::unique_ptr<AlarmsModel> m_model;
    DB* m_db = nullptr;
};

