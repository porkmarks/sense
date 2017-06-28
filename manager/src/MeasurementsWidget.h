#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include "ui_MeasurementsWidget.h"
#include "Comms.h"
#include "DB.h"
#include "DBModel.h"

class MeasurementsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MeasurementsWidget(QWidget *parent = 0);
    void init(Comms& comms, DB& db);

signals:

public slots:

private:

    void refreshFromDB();

    Ui::MeasurementsWidget m_ui;

    Comms* m_comms = nullptr;
    DB* m_db = nullptr;
    std::unique_ptr<DBModel> m_model;
};

