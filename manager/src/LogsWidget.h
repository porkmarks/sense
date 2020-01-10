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
	void init(DB& db);
    void shutdown();

    void setAutoRefresh(bool enabled);

signals:

public slots:
    void refresh();

private:
    void exportData();

    Logger::Filter createFilter() const;

	DB* m_db = nullptr;
    Ui::LogsWidget m_ui;
    std::unique_ptr<LogsModel> m_model;
    QSortFilterProxyModel m_sortingModel;
    std::vector<QMetaObject::Connection> m_uiConnections;
    bool m_sectionSaveScheduled = false;
};

