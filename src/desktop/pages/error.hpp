#pragma once
#include "fundos.hpp"
#include <QWidget>

class ErrorPage : public QWidget {
	Q_OBJECT

public:
	explicit ErrorPage(const fundos::db::status& status, QWidget* parent = nullptr);

signals:
	void retry_requested();
	void migrate_requested();
	void backup_requested();
	void create_new_requested();
	void restore_requested();
	void quit_requested();
};
