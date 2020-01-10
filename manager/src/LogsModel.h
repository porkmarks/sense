#pragma once

#include <memory>
#include <vector>
#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include <QMetaObject>
#include <QTimer>

#include "Logger.h"
#include "Settings.h"

class LogsModel : public QAbstractItemModel
{
    Q_OBJECT
public:

    LogsModel(Settings& settings, Logger& logger);
    ~LogsModel();

    void setFilter(Logger::Filter const& filter);
    void refresh();
    void setAutoRefresh(bool enabled);

    size_t getLineCount() const;
    Logger::LogLine const& getLine(size_t index) const;

    enum class Column
    {
        Timestamp,
        Type,
        Message,
    };

public:
    virtual QModelIndex parent(QModelIndex const& index) const;
    virtual QModelIndex index(int row, int column, QModelIndex const& parent = QModelIndex()) const;

    virtual int rowCount(QModelIndex const& parent = QModelIndex()) const;
    virtual int columnCount(QModelIndex const& parent = QModelIndex()) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation,int role = Qt::DisplayRole) const;
    virtual QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const;

private slots:
    void startAutoRefreshLogLines();
    void refreshLogLines();

private:
    //read-only
    virtual Qt::ItemFlags flags(QModelIndex const& index) const;

    //editing
    virtual bool setData(QModelIndex const& index, QVariant const& value, int role = Qt::EditRole);
    virtual bool setHeaderData(int section, Qt::Orientation orientation, QVariant const& value, int role = Qt::EditRole);
    virtual bool insertColumns(int position, int columns, QModelIndex const& parent = QModelIndex());
    virtual bool removeColumns(int position, int columns, QModelIndex const& parent = QModelIndex());
    virtual bool insertRows(int position, int rows, QModelIndex const& parent = QModelIndex());
    virtual bool removeRows(int position, int rows, QModelIndex const& parent = QModelIndex());

private:
    Settings& m_settings;
    Logger& m_logger;
    Logger::Filter m_filter;
    std::vector<Logger::LogLine> m_logLines;
    QTimer* m_refreshTimer = nullptr;
    QMetaObject::Connection m_autoRefreshConnection;
    bool m_autoRefreshEnabled = false;
};
