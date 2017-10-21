#include "DateTimeFilterWidget.h"
#include <QDateTime>
#include <QCalendarWidget>

DateTimeFilterWidget::DateTimeFilterWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);

    connect(m_ui.usePreset, &QCheckBox::toggled, this, &DateTimeFilterWidget::togglePreset);
    connect(m_ui.preset, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &DateTimeFilterWidget::setPreset);
    connect(m_ui.from, &QDateTimeEdit::editingFinished, this, &DateTimeFilterWidget::fromChanged);
    connect(m_ui.from->calendarWidget(), &QCalendarWidget::clicked, this, &DateTimeFilterWidget::fromChanged);
    connect(m_ui.to, &QDateTimeEdit::editingFinished, this, &DateTimeFilterWidget::toChanged);
    connect(m_ui.to->calendarWidget(), &QCalendarWidget::clicked, this, &DateTimeFilterWidget::toChanged);

    setPreset(m_ui.preset->currentIndex());
}

QDateTime DateTimeFilterWidget::getFromDateTime()
{
    m_emitSignal = false;
    //in case the date changed, reapply it
    if (m_ui.usePreset->isChecked())
    {
        setPreset(m_ui.preset->currentIndex());
    }
    m_emitSignal = true;

    return m_ui.from->dateTime();
}

QDateTime DateTimeFilterWidget::getToDateTime()
{
    m_emitSignal = false;
    //in case the date changed, reapply it
    if (m_ui.usePreset->isChecked())
    {
        setPreset(m_ui.preset->currentIndex());
    }
    m_emitSignal = true;

    return m_ui.to->dateTime();
}

void DateTimeFilterWidget::togglePreset(bool toggled)
{
    if (toggled)
    {
        setPreset(m_ui.preset->currentIndex());
    }
}

void DateTimeFilterWidget::setPreset(int preset)
{
    switch (preset)
    {
    case 0: setPresetToday(); break;
    case 1: setPresetYesterday(); break;
    case 2: setPresetThisWeek(); break;
    case 3: setPresetLastWeek(); break;
    case 4: setPresetThisMonth(); break;
    case 5: setPresetLastMonth(); break;
    case 6: setPresetThisYear(); break;
    case 7: setPresetLastYear(); break;
    }
}
void DateTimeFilterWidget::setPresetToday()
{
    QDateTime dt = QDateTime::currentDateTime();

    dt.setTime(QTime(23, 59, 59, 999));
    m_ui.to->blockSignals(true);
    m_ui.to->setDateTime(dt);
    m_ui.to->blockSignals(false);

    dt.setTime(QTime(0, 0));
    m_ui.from->blockSignals(true);
    m_ui.from->setDateTime(dt);
    m_ui.from->blockSignals(false);

    if (m_emitSignal)
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::setPresetYesterday()
{
    QDateTime dt = QDateTime::currentDateTime();

    dt.setDate(dt.date().addDays(-1));

    dt.setTime(QTime(23, 59, 59, 999));
    m_ui.to->blockSignals(true);
    m_ui.to->setDateTime(dt);
    m_ui.to->blockSignals(false);

    dt.setTime(QTime(0, 0));
    m_ui.from->blockSignals(true);
    m_ui.from->setDateTime(dt);
    m_ui.from->blockSignals(false);

    if (m_emitSignal)
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::setPresetThisWeek()
{
    QDateTime dt = QDateTime::currentDateTime();

    dt.setDate(dt.date().addDays(-dt.date().dayOfWeek()));
    dt.setTime(QTime(0, 0));
    m_ui.from->blockSignals(true);
    m_ui.from->setDateTime(dt);
    m_ui.from->blockSignals(false);

    dt.setDate(dt.date().addDays(6));
    dt.setTime(QTime(23, 59, 59, 999));
    m_ui.to->blockSignals(true);
    m_ui.to->setDateTime(dt);
    m_ui.to->blockSignals(false);

    if (m_emitSignal)
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::setPresetLastWeek()
{
    QDateTime dt = QDateTime::currentDateTime();

    dt.setDate(dt.date().addDays(-dt.date().dayOfWeek()).addDays(-7));
    dt.setTime(QTime(0, 0));
    m_ui.from->blockSignals(true);
    m_ui.from->setDateTime(dt);
    m_ui.from->blockSignals(false);

    dt.setDate(dt.date().addDays(6));
    dt.setTime(QTime(23, 59, 59, 999));
    m_ui.to->blockSignals(true);
    m_ui.to->setDateTime(dt);
    m_ui.to->blockSignals(false);

    if (m_emitSignal)
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::setPresetThisMonth()
{
    QDate date = QDate::currentDate();
    date = QDate(date.year(), date.month(), 1);

    QDateTime dt(date, QTime(0, 0));
    m_ui.from->blockSignals(true);
    m_ui.from->setDateTime(dt);
    m_ui.from->blockSignals(false);

    dt = QDateTime(date.addMonths(1).addDays(-1), QTime(23, 59, 59, 999));
    m_ui.to->blockSignals(true);
    m_ui.to->setDateTime(dt);
    m_ui.to->blockSignals(false);

    if (m_emitSignal)
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::setPresetLastMonth()
{
    QDate date = QDate::currentDate();
    date = QDate(date.year(), date.month(), 1).addMonths(-1);

    QDateTime dt(date, QTime(0, 0));
    m_ui.from->blockSignals(true);
    m_ui.from->setDateTime(dt);
    m_ui.from->blockSignals(false);

    dt = QDateTime(date.addMonths(1).addDays(-1), QTime(23, 59, 59, 999));
    m_ui.to->blockSignals(true);
    m_ui.to->setDateTime(dt);
    m_ui.to->blockSignals(false);

    if (m_emitSignal)
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::setPresetThisYear()
{
    QDate date = QDate::currentDate();
    date = QDate(date.year(), 1, 1);

    QDateTime dt(date, QTime(0, 0));
    m_ui.from->blockSignals(true);
    m_ui.from->setDateTime(dt);
    m_ui.from->blockSignals(false);

    dt = QDateTime(date.addYears(1).addDays(-1), QTime(23, 59, 59, 999));
    m_ui.to->blockSignals(true);
    m_ui.to->setDateTime(dt);
    m_ui.to->blockSignals(false);

    if (m_emitSignal)
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::setPresetLastYear()
{
    QDate date = QDate::currentDate();
    date = QDate(date.year(), 1, 1).addYears(-1);

    QDateTime dt(date, QTime(0, 0));
    m_ui.from->blockSignals(true);
    m_ui.from->setDateTime(dt);
    m_ui.from->blockSignals(false);

    dt = QDateTime(date.addYears(1).addDays(-1), QTime(23, 59, 59, 999));
    m_ui.to->blockSignals(true);
    m_ui.to->setDateTime(dt);
    m_ui.to->blockSignals(false);

    if (m_emitSignal)
    {
        emit filterChanged();
    }
}

void DateTimeFilterWidget::fromChanged()
{
    QDateTime value = m_ui.from->dateTime();
    if (value >= m_ui.to->dateTime())
    {
        QDateTime dt = value;
        dt.setTime(dt.time().addSecs(3600));
        m_ui.to->blockSignals(true);
        m_ui.to->setDateTime(dt);
        m_ui.to->blockSignals(false);
    }

    if (m_emitSignal)
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::toChanged()
{
    QDateTime value = m_ui.to->dateTime();
    if (value <= m_ui.from->dateTime())
    {
        QDateTime dt = value;
        dt.setTime(dt.time().addSecs(-3600));
        m_ui.from->blockSignals(true);
        m_ui.from->setDateTime(dt);
        m_ui.from->blockSignals(false);
    }

    if (m_emitSignal)
    {
        emit filterChanged();
    }
}
