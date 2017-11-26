#include "UsersModel.h"

#include <QWidget>
#include <QIcon>

static std::array<const char*, 3> s_headerNames = {"Id", "Name", "Type"};

//////////////////////////////////////////////////////////////////////////

UsersModel::UsersModel(Settings& settings)
    : QAbstractItemModel()
    , m_settings(settings)
{
}

//////////////////////////////////////////////////////////////////////////

UsersModel::~UsersModel()
{
}

//////////////////////////////////////////////////////////////////////////

void UsersModel::refresh()
{
    beginResetModel();
    endResetModel();

    Q_EMIT layoutChanged();
}

//////////////////////////////////////////////////////////////////////////

QModelIndex UsersModel::index(int row, int column, QModelIndex const& parent) const
{
    if (row < 0 || column < 0)
    {
        return QModelIndex();
    }
    if (!hasIndex(row, column, parent))
    {
        return QModelIndex();
    }
    return createIndex(row, column, nullptr);
}

//////////////////////////////////////////////////////////////////////////

QModelIndex UsersModel::parent(QModelIndex const& /*index*/) const
{
    return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////

int UsersModel::rowCount(QModelIndex const& index) const
{
    if (!index.isValid())
    {
        return static_cast<int>(m_settings.getUserCount());
    }
    else
    {
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////

int UsersModel::columnCount(QModelIndex const& /*index*/) const
{
    return static_cast<int>(s_headerNames.size());
}

//////////////////////////////////////////////////////////////////////////

QVariant UsersModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if (static_cast<size_t>(section) < s_headerNames.size())
        {
            return s_headerNames[section];
        }
    }
    return QAbstractItemModel::headerData(section, orientation, role);
}

//////////////////////////////////////////////////////////////////////////

QVariant UsersModel::data(QModelIndex const& index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    if (static_cast<size_t>(index.row()) >= m_settings.getUserCount())
    {
        return QVariant();
    }

    Settings::User const& user = m_settings.getUser(index.row());
    Settings::UserDescriptor const& descriptor = user.descriptor;

    Column column = static_cast<Column>(index.column());
    if (role == Qt::DisplayRole)
    {
        if (column == Column::Id)
        {
            return user.id;
        }
        else if (column == Column::Name)
        {
            return descriptor.name.c_str();
        }
        else if (column == Column::Type)
        {
            if (descriptor.type == Settings::UserDescriptor::Type::Admin)
            {
                return "Admin";
            }
            return "Normal";
        }
    }
    else if (role == Qt::DecorationRole)
    {
        if (column == Column::Id)
        {
            return QIcon(":/icons/ui/user.png");
        }
    }

    return QVariant();
}

//////////////////////////////////////////////////////////////////////////

Qt::ItemFlags UsersModel::flags(QModelIndex const& /*index*/) const
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

//////////////////////////////////////////////////////////////////////////

bool UsersModel::setData(QModelIndex const& /*index*/, QVariant const& /*value*/, int /*role*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool UsersModel::setHeaderData(int /*section*/, Qt::Orientation /*orientation*/, QVariant const& /*value*/, int /*role*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool UsersModel::insertColumns(int /*position*/, int /*columns*/, QModelIndex const& /*parent*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool UsersModel::removeColumns(int /*position*/, int /*columns*/, QModelIndex const& /*parent*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool UsersModel::insertRows(int /*position*/, int /*rows*/, QModelIndex const& /*parent*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool UsersModel::removeRows(int /*position*/, int /*rows*/, QModelIndex const& /*parent*/)
{
    return false;
}
