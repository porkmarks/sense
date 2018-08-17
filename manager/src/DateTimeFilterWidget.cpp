#include "DateTimeFilterWidget.h"
#include <QDateTime>
#include <QCalendarWidget>
#include <QSettings>

DateTimeFilterWidget::DateTimeFilterWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);

    connect(m_ui.usePreset, &QCheckBox::stateChanged, this, &DateTimeFilterWidget::togglePreset);
    connect(m_ui.preset, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &DateTimeFilterWidget::setPreset);
    connect(m_ui.from, &QDateTimeEdit::editingFinished, this, &DateTimeFilterWidget::fromChanged);
    connect(m_ui.from->calendarWidget(), &QCalendarWidget::clicked, this, &DateTimeFilterWidget::fromChanged);
    connect(m_ui.to, &QDateTimeEdit::editingFinished, this, &DateTimeFilterWidget::toChanged);
    connect(m_ui.to->calendarWidget(), &QCalendarWidget::clicked, this, &DateTimeFilterWidget::toChanged);

    setPreset(m_ui.preset->currentIndex());

    m_timer = new QTimer(this);
    m_timer->setSingleShot(false);
    m_timer->setInterval(60 * 1000); //every minute
    m_timer->start();
    connect(m_timer, &QTimer::timeout, this, &DateTimeFilterWidget::refreshPreset);
}

QDateTime DateTimeFilterWidget::getFromDateTime()
{
    return m_ui.from->dateTime();
}

QDateTime DateTimeFilterWidget::getToDateTime()
{
    return m_ui.to->dateTime();
}

void DateTimeFilterWidget::refreshPreset()
{
    //in case the date changed, reapply it
    if (m_ui.usePreset->isChecked())
    {
        setPreset(m_ui.preset->currentIndex());
    }
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
    case 2: setPresetLastHour(); break;
    case 3: setPresetLast24Hours(); break;
    case 4: setPresetThisWeek(); break;
    case 5: setPresetLastWeek(); break;
    case 6: setPresetThisMonth(); break;
    case 7: setPresetLastMonth(); break;
    case 8: setPresetThisYear(); break;
    case 9: setPresetLastYear(); break;
    }
}
void DateTimeFilterWidget::setPresetToday()
{
    QDateTime fromDT = getFromDateTime();
    QDateTime toDT = getToDateTime();

    QDateTime dt = QDateTime::currentDateTime();

    dt.setTime(QTime(23, 59, 59, 999));
    m_ui.to->blockSignals(true);
    m_ui.to->setDateTime(dt);
    m_ui.to->blockSignals(false);

    dt.setTime(QTime(0, 0));
    m_ui.from->blockSignals(true);
    m_ui.from->setDateTime(dt);
    m_ui.from->blockSignals(false);

    if (m_emitSignal && (fromDT != getFromDateTime() || toDT != getToDateTime()))
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::setPresetYesterday()
{
    QDateTime fromDT = getFromDateTime();
    QDateTime toDT = getToDateTime();

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

    if (m_emitSignal && (fromDT != getFromDateTime() || toDT != getToDateTime()))
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::setPresetLastHour()
{
    QDateTime fromDT = getFromDateTime();
    QDateTime toDT = getToDateTime();

    QDateTime dt = QDateTime::currentDateTime();

    m_ui.to->blockSignals(true);
    m_ui.to->setDateTime(dt);
    m_ui.to->blockSignals(false);

    m_ui.from->blockSignals(true);
    m_ui.from->setDateTime(dt.addSecs(-3600));
    m_ui.from->blockSignals(false);

    if (m_emitSignal && (fromDT != getFromDateTime() || toDT != getToDateTime()))
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::setPresetLast24Hours()
{
    QDateTime fromDT = getFromDateTime();
    QDateTime toDT = getToDateTime();

    QDateTime dt = QDateTime::currentDateTime();

    m_ui.to->blockSignals(true);
    m_ui.to->setDateTime(dt);
    m_ui.to->blockSignals(false);

    m_ui.from->blockSignals(true);
    m_ui.from->setDateTime(dt.addSecs(-3600 * 24));
    m_ui.from->blockSignals(false);

    if (m_emitSignal && (fromDT != getFromDateTime() || toDT != getToDateTime()))
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::setPresetThisWeek()
{
    QDateTime fromDT = getFromDateTime();
    QDateTime toDT = getToDateTime();

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

    if (m_emitSignal && (fromDT != getFromDateTime() || toDT != getToDateTime()))
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::setPresetLastWeek()
{
    QDateTime fromDT = getFromDateTime();
    QDateTime toDT = getToDateTime();

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

    if (m_emitSignal && (fromDT != getFromDateTime() || toDT != getToDateTime()))
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::setPresetThisMonth()
{
    QDateTime fromDT = getFromDateTime();
    QDateTime toDT = getToDateTime();

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

    if (m_emitSignal && (fromDT != getFromDateTime() || toDT != getToDateTime()))
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::setPresetLastMonth()
{
    QDateTime fromDT = getFromDateTime();
    QDateTime toDT = getToDateTime();

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

    if (m_emitSignal && (fromDT != getFromDateTime() || toDT != getToDateTime()))
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::setPresetThisYear()
{
    QDateTime fromDT = getFromDateTime();
    QDateTime toDT = getToDateTime();

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

    if (m_emitSignal && (fromDT != getFromDateTime() || toDT != getToDateTime()))
    {
        emit filterChanged();
    }
}
void DateTimeFilterWidget::setPresetLastYear()
{
    QDateTime fromDT = getFromDateTime();
    QDateTime toDT = getToDateTime();

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

    if (m_emitSignal && (fromDT != getFromDateTime() || toDT != getToDateTime()))
    {
        emit filterChanged();
    }
}

void DateTimeFilterWidget::fromChanged()
{
    QDateTime fromDT = getFromDateTime();
    QDateTime toDT = getToDateTime();

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
    QDateTime fromDT = getFromDateTime();
    QDateTime toDT = getToDateTime();

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

//////////////////////////////////////////////////////////////////////////

void DateTimeFilterWidget::loadSettings()
{
    QSettings settings;

    QDateTime from = getFromDateTime();
    QDateTime to = getToDateTime();

    m_emitSignal = false;

    m_ui.usePreset->setChecked(settings.value("filter/dateTimeFilter/usePreset", true).toBool());
    m_ui.preset->setCurrentIndex(settings.value("filter/dateTimeFilter/preset", 0).toUInt());
    if (!m_ui.usePreset->isChecked())
    {
        m_ui.from->setDateTime(QDateTime::fromTime_t(settings.value("filter/dateTimeFilter/from", QDateTime::currentDateTime().toTime_t()).toUInt()));
        m_ui.to->setDateTime(QDateTime::fromTime_t(settings.value("filter/dateTimeFilter/to", QDateTime::currentDateTime().toTime_t()).toUInt()));
    }

    m_emitSignal = true;

    if (from != getFromDateTime() || to != getToDateTime())
    {
        emit filterChanged();
    }
}

//////////////////////////////////////////////////////////////////////////

void DateTimeFilterWidget::saveSettings()
{
    QSettings settings;
    settings.setValue("filter/dateTimeFilter/usePreset", m_ui.usePreset->isChecked());
    settings.setValue("filter/dateTimeFilter/preset", m_ui.preset->currentIndex());
    settings.setValue("filter/dateTimeFilter/from", m_ui.from->dateTime().toTime_t());
    settings.setValue("filter/dateTimeFilter/to", m_ui.to->dateTime().toTime_t());
}
