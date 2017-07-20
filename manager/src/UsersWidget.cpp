#include "UsersWidget.h"
#include "ConfigureUserDialog.h"
#include <QMessageBox>

#include "Settings.h"

//////////////////////////////////////////////////////////////////////////

UsersWidget::UsersWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    //setEnabled(false);

    connect(m_ui.add, &QPushButton::released, this, &UsersWidget::addUser);
    connect(m_ui.remove, &QPushButton::released, this, &UsersWidget::removeUsers);
    connect(m_ui.list, &QTreeView::activated, this, &UsersWidget::configureUser);
}

//////////////////////////////////////////////////////////////////////////

UsersWidget::~UsersWidget()
{
    shutdown();
}

//////////////////////////////////////////////////////////////////////////

void UsersWidget::init(Settings& settings)
{
    setEnabled(true);
    m_settings = &settings;

    m_model.reset(new UsersModel(settings));
    m_ui.list->setModel(m_model.get());

    for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
    {
        m_ui.list->resizeColumnToContents(i);
    }
}

//////////////////////////////////////////////////////////////////////////

void UsersWidget::shutdown()
{
    setEnabled(false);
    m_ui.list->setModel(nullptr);
    m_ui.list->setItemDelegate(nullptr);
    m_model.reset();
    m_settings = nullptr;
}

//////////////////////////////////////////////////////////////////////////

void UsersWidget::configureUser(QModelIndex const& index)
{
    if (!index.isValid() || static_cast<size_t>(index.row()) >= m_settings->getUserCount())
    {
        return;
    }

    Settings::User user = m_settings->getUser(index.row());

    ConfigureUserDialog dialog(*m_settings);
    dialog.setUser(user);

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        user = dialog.getUser();
        m_settings->setUser(user.id, user.descriptor);
        m_model->refresh();

        for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
        {
            m_ui.list->resizeColumnToContents(i);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void UsersWidget::addUser()
{
    ConfigureUserDialog dialog(*m_settings);

    Settings::User user;
    dialog.setUser(user);

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        user = dialog.getUser();
        m_settings->addUser(user.descriptor);
        m_model->refresh();

        for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
        {
            m_ui.list->resizeColumnToContents(i);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void UsersWidget::removeUsers()
{
    QModelIndexList selected = m_ui.list->selectionModel()->selectedIndexes();
    if (selected.isEmpty())
    {
        QMessageBox::critical(this, "Error", "Please select the user you want to remove.");
        return;
    }

    QModelIndex mi = m_model->index(selected.at(0).row(), static_cast<int>(UsersModel::Column::Id));
    Settings::UserId id = m_model->data(mi).toUInt();
    int32_t index = m_settings->findUserIndexById(id);
    if (index < 0)
    {
        QMessageBox::critical(this, "Error", "Invalid user selected.");
        return;
    }

    Settings::User const& user = m_settings->getUser(index);

    int response = QMessageBox::question(this, "Confirmation", QString("Are you sure you want to delete user '%1'").arg(user.descriptor.name.c_str()));
    if (response != QMessageBox::Yes)
    {
        return;
    }

    m_settings->removeUser(index);

    for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
    {
        m_ui.list->resizeColumnToContents(i);
    }
}

//////////////////////////////////////////////////////////////////////////

