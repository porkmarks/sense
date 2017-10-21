#include "ReportsModel.h"

#include <QWidget>
#include <QIcon>
#include <array>

static std::array<const char*, 5> s_headerNames = {"Id", "Name", "Period", "Data"};

//////////////////////////////////////////////////////////////////////////

ReportsModel::ReportsModel(DB& db)
    : QAbstractItemModel()
    , m_db(db)
{
}

//////////////////////////////////////////////////////////////////////////

ReportsModel::~ReportsModel()
{
}

//////////////////////////////////////////////////////////////////////////

void ReportsModel::refresh()
{
    beginResetModel();
    endResetModel();

    Q_EMIT layoutChanged();
}

//////////////////////////////////////////////////////////////////////////

QModelIndex ReportsModel::index(int row, int column, QModelIndex const& parent) const
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

QModelIndex ReportsModel::parent(QModelIndex const& index) const
{
    return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////

int ReportsModel::rowCount(QModelIndex const& index) const
{
    if (!index.isValid())
    {
        return static_cast<int>(m_db.getReportCount());
    }
    else
    {
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////

int ReportsModel::columnCount(QModelIndex const& index) const
{
    return static_cast<int>(s_headerNames.size());
}

//////////////////////////////////////////////////////////////////////////

QVariant ReportsModel::headerData(int section, Qt::Orientation orientation, int role) const
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

QVariant ReportsModel::data(QModelIndex const& index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    if (static_cast<size_t>(index.row()) >= m_db.getReportCount())
    {
        return QVariant();
    }

    DB::Report const& report = m_db.getReport(index.row());
    DB::ReportDescriptor const& descriptor = report.descriptor;

    Column column = static_cast<Column>(index.column());
    if (role == Qt::CheckStateRole)
    {
    }
    else if (role == Qt::DisplayRole)
    {
        if (column == Column::Id)
        {
            return report.id;
        }
        else if (column == Column::Name)
        {
            return descriptor.name.c_str();
        }
        else if (column == Column::Period)
        {
            if (descriptor.period == DB::ReportDescriptor::Period::Daily)
            {
                return "Daily";
            }
            else if (descriptor.period == DB::ReportDescriptor::Period::Weekly)
            {
                return "Weekly";
            }
            else if (descriptor.period == DB::ReportDescriptor::Period::Monthly)
            {
                return "Monthly";
            }
            else
            {
                size_t days = std::chrono::duration_cast<std::chrono::hours>(descriptor.customPeriod).count() / 24;
                return QString("Every %1 %2").arg(days).arg(days == 1 ? "day" : "days");
            }
        }
        else if (column == Column::Data)
        {
            if (descriptor.data == DB::ReportDescriptor::Data::Summary)
            {
                return "Summary";
            }
            else
            {
                return "All";
            }
        }
    }
    else if (role == Qt::DecorationRole)
    {
        if (column == Column::Id)
        {
            return QIcon(":/icons/ui/report.png");
        }
    }

    return QVariant();
}

//////////////////////////////////////////////////////////////////////////

Qt::ItemFlags ReportsModel::flags(QModelIndex const& index) const
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

//////////////////////////////////////////////////////////////////////////

bool ReportsModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool ReportsModel::setHeaderData(int section, Qt::Orientation orientation, QVariant const& value, int role)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool ReportsModel::insertColumns(int position, int columns, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool ReportsModel::removeColumns(int position, int columns, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool ReportsModel::insertRows(int position, int rows, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool ReportsModel::removeRows(int position, int rows, QModelIndex const& parent)
{
    return false;
}
