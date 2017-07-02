#pragma once

#include <QWidget>
#include "ui_AlarmsWidget.h"
#include "ui_AlarmDialog.h"
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

private:

    Ui::AlarmsWidget m_ui;
    std::unique_ptr<AlarmsModel> m_model;
    DB* m_db = nullptr;

    struct AlarmDialog
    {
        AlarmDialog(DB& db)
            : model(db)
            , delegate(sortingModel)
        {}
        QDialog dialog;
        Ui::AlarmDialog ui;
        SensorsModel model;
        QSortFilterProxyModel sortingModel;
        SensorsDelegate delegate;
    };

    void prepareAlarmDialog(AlarmDialog& dialog);
    void getAlarmData(DB::AlarmDescriptor& alarm, AlarmDialog const& dialog);
    void setAlarmData(AlarmDialog& dialog, DB::AlarmDescriptor const& alarm);
};

