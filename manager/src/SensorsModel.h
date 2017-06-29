#pragma once

#include <memory>
#include <vector>
#include <QAbstractItemModel>

#include "Comms.h"
#include "DB.h"

class SensorsModel : public QAbstractItemModel
{
public:
    SensorsModel(Comms& comms, DB& db);
    ~SensorsModel();

    void setShowCheckboxes(bool show);
    void setSensorChecked(Comms::Sensor_Id id, bool checked);
    bool isSensorChecked(Comms::Sensor_Id id) const;

    virtual QModelIndex parent(QModelIndex const& index) const;

public: //signals
    virtual QModelIndex index(int row, int column, QModelIndex const& parent = QModelIndex()) const;

    virtual int rowCount(QModelIndex const& parent = QModelIndex()) const;
    virtual int columnCount(QModelIndex const& parent = QModelIndex()) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation,int role = Qt::DisplayRole) const;
    virtual QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const;

protected slots:
    void sensorAdded(Comms::Sensor const& sensor);
    void bindSensor(std::string const& name);
    void unbindSensor(Comms::Sensor_Id id);

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
    Comms& m_comms;
    DB& m_db;
    bool m_showCheckboxes = false;

    struct SensorData
    {
        bool isChecked = false;
        Comms::Sensor sensor;
    };

    std::vector<SensorData> m_sensors;
};
