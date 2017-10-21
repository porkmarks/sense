#pragma once

#include <memory>
#include <vector>
#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include <QTimer>

#include "DB.h"

class MeasurementsWidget;

class MeasurementsModel : public QAbstractItemModel
{
    friend class MeasurementsWidget;
public:

    MeasurementsModel(DB& db);
    ~MeasurementsModel();

    void setFilter(DB::Filter const& filter);

    size_t getMeasurementCount() const;
    DB::Measurement const& getMeasurement(size_t index) const;

    enum class Column
    {
        Id,
        Sensor,
        Index,
        Timestamp,
        Temperature,
        Humidity,
        Battery,
        Signal,
        Alarms
    };

public slots:
    void refresh();

private:
    virtual QModelIndex parent(QModelIndex const& index) const;
    virtual QModelIndex index(int row, int column, QModelIndex const& parent = QModelIndex()) const;

    virtual int rowCount(QModelIndex const& parent = QModelIndex()) const;
    virtual int columnCount(QModelIndex const& parent = QModelIndex()) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation,int role = Qt::DisplayRole) const;
    virtual QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const;

private slots:
    void startAutoRefresh(DB::SensorId sensorId);

protected:
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
    DB& m_db;
    DB::Filter m_filter;
    std::vector<DB::Measurement> m_measurements;
    QTimer* m_refreshTimer = nullptr;
};
