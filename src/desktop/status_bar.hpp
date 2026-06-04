#pragma once
#include "fundos.hpp"
#include <QStatusBar>
#include <QLabel>
#include <QToolButton>

class StatusBar : public QStatusBar {
	Q_OBJECT

	QWidget*     dot  = nullptr;
	QLabel*      text = nullptr;
	QToolButton* db_button = nullptr;

	std::shared_ptr<fundos::db> database = nullptr;

	void apply_ready();
	void apply_yellow(const QString& message);
	void apply_red(const QString& message);
	void apply_error(const QString& message);

public:
	explicit StatusBar(QWidget* parent = nullptr);

	void set_database(std::shared_ptr<fundos::db> db);
	void set_status(const fundos::db::outcome& outcome);

private slots:
	void show_db_menu();

signals:
	void backup_requested();
	void restore_requested();
	void create_new_requested();
	void manage_locale_requested();
};
