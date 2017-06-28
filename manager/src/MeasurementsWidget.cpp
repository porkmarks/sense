#include "MeasurementsWidget.h"
#include <iostream>
#include <QDateTime>

MeasurementsWidget::MeasurementsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
}

void MeasurementsWidget::init(Comms& comms, DB& db)
{
    m_comms = &comms;
    m_db = &db;

    m_model.reset(new DBModel(comms, *m_db));
    m_ui.list->setModel(m_model.get());

    m_ui.list->setUniformRowHeights(true);

    DB::Clock::time_point start = DB::Clock::now();
    for (size_t sid = 0; sid < 4; sid++)
    {
        for (size_t index = 0; index < 1000; index++)
        {
            DB::Measurement m;
            m.sensor_id = sid;
            m.index = index;
            m.vcc = std::min((index / 1000.f) + 2.f, 3.3f);
            m_db->add_measurement(m);
        }
    }
    std::cout << "Time to add: " << (std::chrono::duration<float>(DB::Clock::now() - start).count()) << "\n";

    DB::Filter filter;
    start = DB::Clock::now();
    for (size_t i = 0; i < 1; i++)
    {
        std::vector<DB::Measurement> result = m_db->get_filtered_measurements(filter);
    }
    std::cout << "Time to filter: " << (std::chrono::duration<float>(DB::Clock::now() - start).count()) << "\n";

    std::cout.flush();

    refreshFromDB();
}

void MeasurementsWidget::refreshFromDB()
{
    DB::Filter filter;
    m_model->setFilter(filter);
}
