#pragma once

#include <QWidget>
#include "ui_DateTimeFilterWidget.h"

class DateTimeFilterWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DateTimeFilterWidget(QWidget *parent = nullptr);

    QDateTime getFromDateTime();
    QDateTime getToDateTime();

signals:
    void filterChanged();

private slots:
    void togglePreset(bool toggled);
    void setPreset(int preset);
    void setPresetToday();
    void setPresetYesterday();
    void setPresetThisWeek();
    void setPresetLastWeek();
    void setPresetThisMonth();
    void setPresetLastMonth();
    void setPresetThisYear();
    void setPresetLastYear();

    void fromChanged();
    void toChanged();

private:
    Ui::DateTimeFilterWidget m_ui;
    bool m_emitSignal = true;
};
