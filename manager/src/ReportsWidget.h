#pragma once

#include <QWidget>
#include "ui_ReportsWidget.h"
#include "DB.h"
#include "ReportsModel.h"

#include "SensorsModel.h"
#include "SensorsDelegate.h"
#include <QSortFilterProxyModel>

class ReportsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ReportsWidget(QWidget *parent = 0);
    ~ReportsWidget();
    void init(DB& db);
    void shutdown();

signals:

private slots:
    void addReport();
    void removeReports();
    void configureReport(QModelIndex const& index);

private:

    Ui::ReportsWidget m_ui;
    std::unique_ptr<ReportsModel> m_model;
    DB* m_db = nullptr;
};

