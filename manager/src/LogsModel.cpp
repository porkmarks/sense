#include "LogsModel.h"

#include <QWidget>
#include <QIcon>
#include <QDateTime>

#include <bitset>
#include <array>
#include "Utils.h"

static std::array<const char*, 3> s_headerNames = {"Timestamp", "Type", "Message"};

//////////////////////////////////////////////////////////////////////////

LogsModel::LogsModel(DB& db, Logger& logger)
    : m_db(db)
    , m_logger(logger)
{
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);

    connect(m_refreshTimer, &QTimer::timeout, this, &LogsModel::refreshLogLines);
}

//////////////////////////////////////////////////////////////////////////

LogsModel::~LogsModel()
{
}

//////////////////////////////////////////////////////////////////////////

void LogsModel::setAutoRefresh(bool enabled)
{
    if (enabled != m_autoRefreshEnabled)
    {
        m_autoRefreshEnabled = enabled;
        if (enabled)
            m_autoRefreshConnection = connect(&m_logger, &Logger::logsChanged, this, &LogsModel::startAutoRefreshLogLines);
        else
            QObject::disconnect(m_autoRefreshConnection);
    }
}

//////////////////////////////////////////////////////////////////////////

QModelIndex LogsModel::index(int row, int column, QModelIndex const& parent) const
{
    if (row < 0 || column < 0)
        return QModelIndex();

    if (!hasIndex(row, column, parent))
        return QModelIndex();

    return createIndex(row, column, nullptr);
}

//////////////////////////////////////////////////////////////////////////

QModelIndex LogsModel::parent(QModelIndex const& /*index*/) const
{
    return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////

int LogsModel::rowCount(QModelIndex const& index) const
{
    if (!index.isValid())
        return static_cast<int>(m_logLines.size());
    else
        return 0;
}

//////////////////////////////////////////////////////////////////////////

int LogsModel::columnCount(QModelIndex const& /*index*/) const
{
    return static_cast<int>(s_headerNames.size());
}

//////////////////////////////////////////////////////////////////////////

QVariant LogsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if (static_cast<size_t>(section) < s_headerNames.size())
            return s_headerNames[section];
    }
    return QAbstractItemModel::headerData(section, orientation, role);
}

//////////////////////////////////////////////////////////////////////////

QVariant LogsModel::data(QModelIndex const& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    size_t indexRow = static_cast<size_t>(index.row());
    if (indexRow >= m_logLines.size())
        return QVariant();

    Logger::LogLine const& line = m_logLines[indexRow];

    Column column = static_cast<Column>(index.column());

    if (role == Qt::UserRole + 5) // sorting
    {
        if (column == Column::Timestamp)
            return qulonglong(line.index);
        else if (column == Column::Type)
            return static_cast<int>(line.type);
        else if (column == Column::Message)
            return line.message.c_str();

        return data(index, Qt::DisplayRole);
    }
    else if (role == Qt::DecorationRole)
    {
        if (column == Column::Type)
        {
            static QIcon vicon(":/icons/ui/verbose.png");
            static QIcon iicon(":/icons/ui/info.png");
            static QIcon wicon(":/icons/ui/warning.png");
            static QIcon eicon(":/icons/ui/error.png");
            switch (line.type)
            {
            case Logger::Type::VERBOSE: return vicon;
            case Logger::Type::INFO: return iicon;
            case Logger::Type::WARNING: return wicon;
            case Logger::Type::CRITICAL: return eicon;
            default: QVariant();
            }
        }
    }
    else if (role == Qt::DisplayRole)
    {
        if (column == Column::Timestamp)
            return utils::toString<Logger::Clock>(line.timePoint, m_db.getGeneralSettings().dateTimeFormat);
        else if (column == Column::Type)
        {
            switch (line.type)
            {
            case Logger::Type::VERBOSE: return "Verbose";
            case Logger::Type::INFO: return "Info";
            case Logger::Type::WARNING: return "Warning";
            case Logger::Type::CRITICAL: return "Error";
            default: return "N/A";
            }
        }
        else if (column == Column::Message)
            return line.message.c_str();
    }

    return QVariant();
}

//////////////////////////////////////////////////////////////////////////

Qt::ItemFlags LogsModel::flags(QModelIndex const& /*index*/) const
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

//////////////////////////////////////////////////////////////////////////

void LogsModel::setFilter(Logger::Filter const& filter)
{
    m_filter = filter;
    refreshLogLines();
}

//////////////////////////////////////////////////////////////////////////

void LogsModel::startAutoRefreshLogLines()
{
    m_refreshTimer->start(5000);
}

//////////////////////////////////////////////////////////////////////////

void LogsModel::refreshLogLines()
{
    beginResetModel();
    m_logLines = m_logger.getFilteredLogLines(m_filter);
    endResetModel();

    Q_EMIT layoutChanged();
}

//////////////////////////////////////////////////////////////////////////

size_t LogsModel::getLineCount() const
{
    return m_logLines.size();
}

//////////////////////////////////////////////////////////////////////////

Logger::LogLine const& LogsModel::getLine(size_t index) const
{
    return m_logLines[index];
}

//////////////////////////////////////////////////////////////////////////

bool LogsModel::setData(QModelIndex const& /*index*/, QVariant const& /*value*/, int /*role*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool LogsModel::setHeaderData(int /*section*/, Qt::Orientation /*orientation*/, QVariant const& /*value*/, int /*role*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool LogsModel::insertColumns(int /*position*/, int /*columns*/, QModelIndex const& /*parent*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool LogsModel::removeColumns(int /*position*/, int /*columns*/, QModelIndex const& /*parent*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool LogsModel::insertRows(int /*position*/, int /*rows*/, QModelIndex const& /*parent*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool LogsModel::removeRows(int /*position*/, int /*rows*/, QModelIndex const& /*parent*/)
{
    return false;
}

