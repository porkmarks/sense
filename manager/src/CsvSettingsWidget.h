#pragma once

#include <QWidget>
#include "ui_CsvSettingsWidget.h"
#include "Comms.h"
#include "DB.h"
#include "DB.h"

#include <QSortFilterProxyModel>

class CsvSettingsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CsvSettingsWidget(QWidget *parent = 0);
    ~CsvSettingsWidget();

	void init(DB& db);

	void setCsvSettings(DB::GeneralSettings const& generalSettings, DB::CsvSettings const& settings);
	DB::CsvSettings getCsvSettings() const;

    void setPreviewVisible(bool value);

signals:
    void settingsChanged();

private:
    void refreshPreview();

    Ui::CsvSettingsWidget m_ui;
    DB* m_db = nullptr;
    DB::GeneralSettings m_generalSettings;
};

