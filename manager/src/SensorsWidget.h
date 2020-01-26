#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>

#include "ui_SensorsWidget.h"
#include "DB.h"
#include "SensorsModel.h"
#include "SensorsDelegate.h"

class SensorsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SensorsWidget(QWidget *parent = 0);
    ~SensorsWidget();
	void init(DB& db);
    void shutdown();

signals:

private slots:
    void bindSensor();
    void removeSensor();
    void rebindSensor();
    void configureSensor(QModelIndex const& index);
	void setPermissions();

private:
    Ui::SensorsWidget m_ui;
    std::unique_ptr<SensorsModel> m_model;
    QSortFilterProxyModel m_sortingModel;
    std::unique_ptr<SensorsDelegate> m_delegate;

    DB* m_db = nullptr;
    std::vector<QMetaObject::Connection> m_uiConnections;
    bool m_sectionSaveScheduled = false;
};

