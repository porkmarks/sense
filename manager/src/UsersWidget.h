#pragma once

#include <QWidget>
#include "ui_UsersWidget.h"
#include "DB.h"
#include "UsersModel.h"

class UsersWidget : public QWidget
{
    Q_OBJECT
public:
    explicit UsersWidget(QWidget *parent = 0);
    ~UsersWidget();
	void init(DB& db);
    void shutdown();

signals:

private slots:
    void addUser();
    void removeUsers();
    void configureUser(QModelIndex const& index);
    void setPermissions();

private:

    Ui::UsersWidget m_ui;
    std::unique_ptr<UsersModel> m_model;
	DB* m_db = nullptr;
    std::vector<QMetaObject::Connection> m_uiConnections;
    bool m_sectionSaveScheduled = false;
};

