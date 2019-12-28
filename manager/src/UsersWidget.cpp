#include "UsersWidget.h"
#include "ConfigureUserDialog.h"
#include "PermissionsCheck.h"
#include "Settings.h"

#include <QMessageBox>
#include <QSettings>

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
    for (const QMetaObject::Connection& connection: m_uiConnections)
    {
        QObject::disconnect(connection);
    }
    m_uiConnections.clear();

    setEnabled(true);

    m_settings = &settings;

    m_model.reset(new UsersModel(settings));
    m_ui.list->setModel(m_model.get());
    m_ui.list->setUniformRowHeights(true);

    connect(m_ui.list->header(), &QHeaderView::sectionResized, [this]()
    {
        if (!m_sectionSaveScheduled)
        {
            m_sectionSaveScheduled = true;
            QTimer::singleShot(500, [this]()
            {
                m_sectionSaveScheduled = false;
                QSettings settings;
                settings.setValue("users/list/state", m_ui.list->header()->saveState());
            });
        }
    });

    {
        QSettings settings;
        m_ui.list->header()->restoreState(settings.value("users/list/state").toByteArray());
    }

    setPermissions();
    m_uiConnections.push_back(connect(&settings, &Settings::userLoggedIn, this, &UsersWidget::setPermissions));
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

void UsersWidget::setPermissions()
{
}

//////////////////////////////////////////////////////////////////////////

void UsersWidget::configureUser(QModelIndex const& index)
{
    size_t indexRow = static_cast<size_t>(index.row());
    if (!index.isValid() || indexRow >= m_settings->getUserCount())
    {
        return;
    }

    Settings::User user = m_settings->getUser(indexRow);
    if (m_settings->getLoggedInUserId() != user.id && 
        !hasPermissionOrCanLoginAsAdmin(*m_settings, Settings::UserDescriptor::PermissionChangeUsers, this))
    {
        QMessageBox::critical(this, "Error", "You don't have permission to configure users.");
        return;
    }

    ConfigureUserDialog dialog(*m_settings, this);
    dialog.setUser(user);

    if (user.descriptor.type == Settings::UserDescriptor::Type::Admin)
    {
        dialog.setForcedType(Settings::UserDescriptor::Type::Admin);
    }
    else if (!m_settings->needsAdmin())
    {
        dialog.setForcedType(Settings::UserDescriptor::Type::Normal);
    }

    do
	{
		int result = dialog.exec();
		if (result == QDialog::Accepted)
		{
			user = dialog.getUser();
			Result<void> result = m_settings->setUser(user.id, user.descriptor);
			if (result != success)
			{
				QMessageBox::critical(this, "Error", QString("Cannot change user '%1': %2").arg(user.descriptor.name.c_str()).arg(result.error().what().c_str()));
				continue;
			}
			m_model->refresh();
		}
        break;
    } while (true);
}

//////////////////////////////////////////////////////////////////////////

void UsersWidget::addUser()
{
    if (!hasPermissionOrCanLoginAsAdmin(*m_settings, Settings::UserDescriptor::PermissionAddRemoveUsers, this))
    {
        QMessageBox::critical(this, "Error", "You don't have permission to add users.");
        return;
    }

    ConfigureUserDialog dialog(*m_settings, this);

    Settings::User user;
    dialog.setUser(user);

    if (user.descriptor.type == Settings::UserDescriptor::Type::Admin)
    {
        dialog.setForcedType(Settings::UserDescriptor::Type::Admin);
    }
    else if (!m_settings->needsAdmin())
    {
        dialog.setForcedType(Settings::UserDescriptor::Type::Normal);
    }

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        user = dialog.getUser();
        m_settings->addUser(user.descriptor);
        m_model->refresh();
    }
}

//////////////////////////////////////////////////////////////////////////

void UsersWidget::removeUsers()
{
    if (!hasPermissionOrCanLoginAsAdmin(*m_settings, Settings::UserDescriptor::PermissionAddRemoveUsers, this))
    {
        QMessageBox::critical(this, "Error", "You don't have permission to remove users.");
        return;
    }

    QModelIndexList selected = m_ui.list->selectionModel()->selectedIndexes();
    if (selected.isEmpty())
    {
        QMessageBox::critical(this, "Error", "Please select the user you want to remove.");
        return;
    }

    QModelIndex mi = m_model->index(selected.at(0).row(), static_cast<int>(UsersModel::Column::Id));
    Settings::UserId id = m_model->data(mi).toUInt();
    int32_t _index = m_settings->findUserIndexById(id);
    if (_index < 0)
    {
        QMessageBox::critical(this, "Error", "Invalid user selected.");
        return;
    }

    size_t index = static_cast<size_t>(_index);
    Settings::User const& user = m_settings->getUser(index);

    int response = QMessageBox::question(this, "Confirmation", QString("Are you sure you want to delete user '%1'").arg(user.descriptor.name.c_str()));
    if (response != QMessageBox::Yes)
    {
        return;
    }

    m_settings->removeUser(index);
}

//////////////////////////////////////////////////////////////////////////

