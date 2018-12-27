#pragma once

#include <QWidget>
#include <QSortFilterProxyModel>
#include "DB.h"
#include "SensorsModel.h"
#include "SensorsDelegate.h"
#include "ui_SensorsFilterWidget.h"

class SensorsFilterWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SensorsFilterWidget(QWidget *parent = nullptr);

    void init(DB& db);

    SensorsModel& getSensorModel();

signals:

private:
    DB* m_db = nullptr;
    std::unique_ptr<SensorsModel> m_model;
    std::unique_ptr<QSortFilterProxyModel> m_sortingModel;
    std::unique_ptr<SensorsDelegate> m_delegate;
    Ui::SensorsFilterWidget m_ui;
    bool m_sectionSaveScheduled = false;
};
