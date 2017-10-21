#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include "ui_LogsWidget.h"
#include "DB.h"
#include "LogsModel.h"

class LogsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LogsWidget(QWidget *parent = 0);
    ~LogsWidget();
    void init();
    void shutdown();

signals:

private slots:
    void refresh();

private:
    void exportData();

    Logger::Filter createFilter() const;

    Ui::LogsWidget m_ui;
    std::unique_ptr<LogsModel> m_model;
    QSortFilterProxyModel m_sortingModel;
};

