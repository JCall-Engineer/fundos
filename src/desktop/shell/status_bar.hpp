#pragma once
#include "database.hpp"
#include <QStatusBar>
#include <QLabel>
#include <QToolButton>

class StatusBar : public QStatusBar {
	Q_OBJECT

	QWidget*     dot          = nullptr;
	QLabel*      text         = nullptr;
	QToolButton* db_button    = nullptr;
	QLabel*      info_size    = nullptr;
	QLabel*      info_journal = nullptr;
	QLabel*      info_schema  = nullptr;

	bool is_connected = false;

	void apply_ready();
	void apply_yellow(const QString& message);
	void apply_red(const QString& message);
	void apply_error(const QString& message);

public:
	explicit StatusBar(QWidget* parent = nullptr);

	void set_status(const fundos::db::outcome& outcome);

private slots:
	void show_db_menu();

public slots:
	void on_db_open(fundos::db::status open_result);
	void on_db_info(AppDatabase::DatabaseInfo info);

signals:
	void db_info_requested();
	void backup_requested();
	void restore_requested();
	void create_new_requested();
	void manage_locale_requested();
};
