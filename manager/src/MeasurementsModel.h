#pragma once

#include <memory>
#include <vector>
#include <QAbstractItemModel>

#include "Comms.h"
#include "DB.h"

class MeasurementsModel : public QAbstractItemModel
{
public:

    MeasurementsModel(Comms& comms, DB& db);
    ~MeasurementsModel();

    void setFilter(DB::Filter const& filter);
    void refresh();
    size_t getMeasurementCount() const;

    virtual QModelIndex parent(QModelIndex const& index) const;

public: //signals
    virtual QModelIndex index(int row, int column, QModelIndex const& parent = QModelIndex()) const;

    virtual int rowCount(QModelIndex const& parent = QModelIndex()) const;
    virtual int columnCount(QModelIndex const& parent = QModelIndex()) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation,int role = Qt::DisplayRole) const;
    virtual QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const;

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
    DB::Filter m_filter;
    std::vector<DB::Measurement> m_measurements;
};
