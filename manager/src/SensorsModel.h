#pragma once

#include <memory>
#include <vector>
#include <QAbstractItemModel>
#include <QStyledItemDelegate>

#include "DB.h"

class SensorsModel : public QAbstractItemModel
{
public:
    SensorsModel(DB& db);
    ~SensorsModel();

    void setShowCheckboxes(bool show);
    void setSensorChecked(DB::SensorId id, bool checked);
    bool isSensorChecked(DB::SensorId id) const;

    enum class Column
    {
        Name,
        Id,
        SerialNumber,
        Temperature,
        Humidity,
        Battery,
        Signal,
        State,
        SensorErrors,
        Alarms
    };

    size_t getSensorCount() const;
    DB::Sensor const& getSensor(size_t index) const;

public:
    virtual QModelIndex parent(QModelIndex const& index) const;
    virtual QModelIndex index(int row, int column, QModelIndex const& parent = QModelIndex()) const;

    virtual int rowCount(QModelIndex const& parent = QModelIndex()) const;
    virtual int columnCount(QModelIndex const& parent = QModelIndex()) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation,int role = Qt::DisplayRole) const;
    virtual QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const;

protected slots:
    void sensorAdded(DB::SensorId id);
    void sensorChanged(DB::SensorId id);
    void sensorRemoved(DB::SensorId id);

    void bindSensor(std::string const& name);
    void unbindSensor(DB::SensorId id);

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

    bool m_showCheckboxes = false;

    struct SensorData
    {
        bool isChecked = false;
        DB::SensorId sensorId;
    };

    std::vector<SensorData> m_sensors;
};
