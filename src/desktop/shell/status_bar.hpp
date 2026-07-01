#pragma once
#include "database.hpp"
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QToolButton>
#include <QStatusBar>

class StatusBar : public QStatusBar {
	Q_OBJECT

	enum class update_state {
		idle,
		checking,
		up_to_date,
		available,
	};
	update_state current_update_state = update_state::idle;

	QNetworkAccessManager* network      = nullptr;
	QNetworkReply*         update_reply = nullptr;

	QWidget*     dot             = nullptr;
	QLabel*      text            = nullptr;
	QToolButton* db_button       = nullptr;
	QToolButton* download_button = nullptr;
	QToolButton* help_button     = nullptr;

	// Nulled out when the menu closes; guards against a slow database response arriving after the menu is gone.
	QLabel*      info_size    = nullptr;
	QLabel*      info_journal = nullptr;
	QLabel*      info_schema  = nullptr;

	bool is_connected = false;

	void open_download();
	void open_guide();

	void apply_ready();
	void apply_yellow(const QString& message);
	void apply_red(const QString& message);

	/// Routes to red if disconnected, yellow if connected (error is transient/recoverable).
	void apply_error(const QString& message);

public:
	explicit StatusBar(QWidget* parent = nullptr);

	void set_status(const fundos::db::outcome& outcome);

private slots:
	void show_db_menu();
	void show_help_menu();

public slots:
	void check_for_updates(bool user_initiated);
	void on_db_open(fundos::db::status open_result);
	void on_db_info(AppDatabase::DatabaseInfo info);

signals:
	void update_available();
	void db_info_requested();
	void backup_requested();
	void restore_requested();
	void create_new_requested();
	void manage_locale_requested();
};
