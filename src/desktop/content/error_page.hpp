#pragma once
#include "fundos.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSvgWidget>
#include <QWidget>

class ErrorPage : public QWidget {
	Q_OBJECT

	enum class button : uint8_t {
		retry,
		migrate,
		backup,
		create_new,
		restore,
		quit,
	};

	struct button_spec {
		QString label;
		void (ErrorPage::*signal)();
	};

	QLabel*      title        = nullptr;
	QLabel*      description  = nullptr;
	QVBoxLayout* buttons      = nullptr;

	void        setup_layout();
	button_spec resolve_button(button which);
	void        add_button(button which);
	void        setup_error(const QString& title_text, QString description_text, const std::optional<fundos::db::message>& msg = std::nullopt);

public:
	explicit ErrorPage(const fundos::db::status& status, QWidget* parent = nullptr);
	explicit ErrorPage(const fundos::db::outcome& outcome, QWidget* parent = nullptr);

signals:
	void retry_requested();
	void migrate_requested();
	void backup_requested();
	void create_new_requested();
	void restore_requested();
	void quit_requested();
};
