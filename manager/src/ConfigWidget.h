#pragma once

#include <QWidget>
#include "ui_ConfigWidget.h"
#include "Comms.h"

class ConfigWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ConfigWidget(QWidget *parent = 0);
    void init(Comms& comms);

signals:

private slots:
    void configReceived(Comms::Config const& config);
    void baseStationDisconnected(Comms::BaseStation const& bs);
    void resetConfig();
    void applyConfig();

private:
    void showConfig(Comms::Config const& config);

    Ui::ConfigWidget m_ui;
    Comms* m_comms = nullptr;
    Comms::Config m_receivedConfig;
    Comms::Config m_crtConfig;
};

