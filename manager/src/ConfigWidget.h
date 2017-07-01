#pragma once

#include <QWidget>
#include "ui_ConfigWidget.h"
#include "DB.h"

class ConfigWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ConfigWidget(QWidget *parent = 0);
    ~ConfigWidget();
    void init(DB& db);
    void shutdown();

signals:

private slots:
    void configChanged();
    void resetConfig();
    void applyConfig();

private:
    void showConfig(DB::Config const& config);

    Ui::ConfigWidget m_ui;
    DB* m_db = nullptr;
    DB::Config m_receivedConfig;
    DB::Config m_crtConfig;
};

