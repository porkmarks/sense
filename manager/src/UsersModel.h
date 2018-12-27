#pragma once

#include <memory>
#include <vector>
#include <QAbstractItemModel>
#include "Settings.h"

class UsersModel : public QAbstractItemModel
{
public:
    UsersModel(Settings& db);
    ~UsersModel();

    enum class Column
    {
        Id,
        Name,
        Type
    };

    void refresh();


    virtual QModelIndex parent(QModelIndex const& index) const;
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
    Settings& m_settings;
};
