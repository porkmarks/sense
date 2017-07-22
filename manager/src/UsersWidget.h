#pragma once

#include <QWidget>
#include "ui_UsersWidget.h"
#include "Settings.h"
#include "UsersModel.h"

class UsersWidget : public QWidget
{
    Q_OBJECT
public:
    explicit UsersWidget(QWidget *parent = 0);
    ~UsersWidget();
    void init(Settings& settings);
    void shutdown();

signals:

private slots:
    void addUser();
    void removeUsers();
    void configureUser(QModelIndex const& index);
    void setRW();

private:

    Ui::UsersWidget m_ui;
    std::unique_ptr<UsersModel> m_model;
    Settings* m_settings = nullptr;
};

