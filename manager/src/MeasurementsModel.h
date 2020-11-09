#pragma once

#include <memory>
#include <vector>
#include <bitset>
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

    void setActive(bool active);

    void setFilter(DB::Filter const& filter);
    DB::Filter const& getFilter() const;

    size_t getMeasurementCount() const;
    DB::Measurement const& getMeasurement(size_t index);
    std::vector<DB::Measurement> getAllMeasurements() const;

    Result<DB::Measurement> getMeasurement(QModelIndex index) const;

    enum class Column
    {
        Id,
        Sensor,
        Index,
        Timestamp,
        ReceivedTimestamp,
        Temperature,
        Humidity,
        Battery,
        Signal,
        Alarms
    };

public slots:
    void refresh();

private:
    QModelIndex parent(QModelIndex const& index) const override;
    QModelIndex index(int row, int column, QModelIndex const& parent = QModelIndex()) const override;

    int rowCount(QModelIndex const& parent = QModelIndex()) const override;
    int columnCount(QModelIndex const& parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation,int role = Qt::DisplayRole) const override;
    QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;

    Qt::ItemFlags flags(QModelIndex const& index) const override;

    //editing
    bool setData(QModelIndex const& index, QVariant const& value, int role = Qt::EditRole) override;
    bool setHeaderData(int section, Qt::Orientation orientation, QVariant const& value, int role = Qt::EditRole) override;
    bool insertColumns(int position, int columns, QModelIndex const& parent = QModelIndex()) override;
    bool removeColumns(int position, int columns, QModelIndex const& parent = QModelIndex()) override;
    bool insertRows(int position, int rows, QModelIndex const& parent = QModelIndex()) override;
    bool removeRows(int position, int rows, QModelIndex const& parent = QModelIndex()) override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
// 	void fetchMore(const QModelIndex& parent) override;
// 	bool canFetchMore(const QModelIndex& parent) const override;

//	void startAutoRefresh(IClock::duration timer);

private slots:
// 	void startSlowAutoRefresh();
// 	void startFastAutoRefresh();

private:
	std::vector<QMetaObject::Connection> m_connections;
    DB& m_db;
    DB::Filter m_filter;
    Column m_sortColumn = Column::Timestamp;
    DB::Filter::SortOrder m_sortOrder = DB::Filter::SortOrder::Descending;
//    size_t m_measurementsStartIndex = 0;
//    size_t m_measurementsTotalCount = 0;
    std::vector<DB::Measurement> m_measurements;
//    QTimer* m_refreshTimer = nullptr;
};
