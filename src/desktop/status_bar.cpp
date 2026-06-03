#include "status_bar.hpp"
#include <QHBoxLayout>
#include <QMenu>
#include <QWidgetAction>

StatusBar::StatusBar(QWidget* parent) : QStatusBar(parent) {
	setSizeGripEnabled(false);

	auto* left = new QWidget(this);
	auto* layout = new QHBoxLayout(left);
	layout->setContentsMargins(4, 0, 0, 0);
	layout->setSpacing(6);

	dot = new QLabel(this);
	dot->setFixedSize(10, 10);
	layout->addWidget(dot);

	text = new QLabel(this);
	layout->addWidget(text);

	addWidget(left);

	db_button = new QToolButton(this);
	db_button->setIcon(QIcon(":/icons/database.svg"));
	db_button->setAutoRaise(true);
	db_button->setFixedSize(24, 24);
	connect(db_button, &QToolButton::clicked, this, &StatusBar::show_db_menu);
	addPermanentWidget(db_button);
}

void StatusBar::apply_ready() {
	if (database == nullptr || !database->is_ready()) {
		return apply_red(tr("disconnected"));
	}
	dot->setStyleSheet("background: #4caf50; border-radius: 5px;");
	text->setText(tr("ready"));
}

void StatusBar::apply_yellow(const QString& message) {
	dot->setStyleSheet("background: #ffc107; border-radius: 5px;");
	text->setText(message);
}

void StatusBar::apply_red(const QString& message) {
	dot->setStyleSheet("background: #f44336; border-radius: 5px;");
	text->setText(message);
}

void StatusBar::apply_error(const QString& message) {
	if (database == nullptr || !database->is_connected()) {
		apply_red(message);
		return;
	}
	apply_yellow(message);
}

void StatusBar::set_database(std::shared_ptr<fundos::db> db) {
	database = db;
	text->setToolTip({});
	db_button->setEnabled(database != nullptr && database->is_connected());

	if (database == nullptr) {
		return apply_red(tr("disconnected"));
	}

	using code = fundos::db::status::code;
	using schema = fundos::db::schema_state;
	using error = fundos::db::error;

	const auto& status = database->get_status();

	switch(status.result) {
		case code::ok:
			return apply_ready();
		case code::needs_migration:
			return apply_error(tr("migration required"));
		case code::null_db:
			return apply_error(tr("db error: null"));
		case code::schema_error: {
			switch(status.schema_status) {
				case schema::newer_schema:
					return apply_error(tr("database requires a newer version of fundos"));
				case schema::schema_mismatch:
					return apply_error(tr("database schema is corrupted"));
				case schema::app_mismatch:
					return apply_error(tr("unrecognized database file"));
				default:
					return apply_error(tr("schema error code: ") + QString::number(static_cast<int>(status.schema_status)));
			}
		}
		case code::sqlite3_error: {
			if (status.sqlite3_outcome.msg.has_value()) {
				const auto& view = status.sqlite3_outcome.msg->view();
				text->setToolTip(QString::fromUtf8(view.data(), view.size()));
			}
			switch(status.sqlite3_outcome.code) {
				case error::corrupted:
					return apply_error(tr("database is corrupted"));
				case error::unavailable:
					return apply_error(tr("database unavailable"));
				case error::readonly:
					return apply_error(tr("database is read-only"));
				case error::out_of_memory:
					return apply_error(tr("out of memory"));
				case error::disk_full:
					return apply_error(tr("disk full"));
				case error::internal:
					return apply_error(tr("unexpected internal error"));
				default:
					return apply_error(tr("db error code: ") + QString::number(static_cast<int>(status.sqlite3_outcome.code)));
			}
		}
	}
}

void StatusBar::set_status(const fundos::db::outcome& outcome) {
	text->setToolTip({});
	db_button->setEnabled(database != nullptr && database->is_connected());

	QString disconnected;
	if (database == nullptr) {
		return apply_red(tr("disconnected"));
	}
	if (!database->is_connected()) {
		disconnected = tr("disconnected") + ": ";
	}

	if (outcome.msg.has_value()) {
		const auto& view = outcome.msg->view();
		text->setToolTip(QString::fromUtf8(view.data(), view.size()));
	}
	using error = fundos::db::error;
	switch(outcome.code) {
		case error::none:
			return apply_ready();
		case error::not_ready:
			return apply_error(disconnected + tr("database is not ready"));
		case error::corrupted:
			return apply_error(disconnected + tr("database is corrupted"));
		case error::unavailable:
			return apply_error(disconnected + tr("database busy, try again"));
		case error::readonly:
			return apply_error(disconnected + tr("database is read-only"));
		case error::out_of_memory:
			return apply_error(disconnected + tr("out of memory"));
		case error::disk_full:
			return apply_error(disconnected + tr("disk full"));
		case error::constraint:
			FUNDOS_ASSERT(false, "constraint violation occurred");
			return apply_error(disconnected + tr("constraint violation"));
		case error::not_found:
			return apply_error(disconnected + tr("record not found"));
		case error::bad_request:
			FUNDOS_ASSERT(false, "bad request occurred");
			return apply_error(disconnected + tr("incorrect API usage"));
		case error::rejected:
			FUNDOS_ASSERT(false, "did not properly validate user input");
			return apply_error(disconnected + tr("attempted illegal operation"));
		case error::internal:
			return apply_error(disconnected + tr("unexpected internal error"));
	}
}

static QString format_size(int64_t bytes) {
	static constexpr std::pair<int64_t, const char*> thresholds[] = {
		{ 1'073'741'824, " GB" },
		{ 1'048'576,     " MB" },
		{ 1'024,         " KB" },
	};
	for (auto [divisor, suffix] : thresholds) {
		if (bytes >= divisor) {
			return QString::number(bytes / static_cast<double>(divisor), 'f', 1) + suffix;
		}
	}
	return QString::number(bytes) + " B";
}

void StatusBar::show_db_menu() {
	QMenu menu(this);
	auto add_info_row = [&](const QString& key, const QString& value) {
		auto* widget = new QWidget(&menu);
		auto* layout = new QHBoxLayout(widget);
		layout->setContentsMargins(16, 4, 16, 4);

		auto* key_label = new QLabel(key, widget);
		auto* value_label = new QLabel(value, widget);
		value_label->setAlignment(Qt::AlignRight);

		layout->addWidget(key_label);
		layout->addWidget(value_label);

		auto* action = new QWidgetAction(&menu);
		action->setDefaultWidget(widget);
		menu.addAction(action);
	};

	// DATABASE section
	menu.addSection(tr("DATABASE"));
	auto size = database->size_on_disk();
	QString size_str = tr("error");
	if (size) {
		size_str = format_size(size.value());
	} else {
		set_status(size.status());
	}

	add_info_row(tr("size on disk"), size_str);
	add_info_row(tr("journal mode"), QString::fromStdString(database->get_status().journal_mode));
	add_info_row(tr("schema version"), QString::number(database->schema_version()));

	// ACTIONS section
	menu.addSection(tr("ACTIONS"));
	connect(menu.addAction(tr("Manage Locale...")), &QAction::triggered, this, &StatusBar::manage_locale_requested);
	connect(menu.addAction(tr("Back Up Database...")), &QAction::triggered, this, &StatusBar::backup_requested);

	menu.addSeparator();
	auto* restore_action = menu.addAction(tr("Restore from Backup..."));
	restore_action->setIcon(QIcon(":/icons/alert-triangle.svg"));
	connect(restore_action, &QAction::triggered, this, &StatusBar::restore_requested);

	auto* replace_action = menu.addAction(tr("Replace with New Database..."));
	replace_action->setIcon(QIcon(":/icons/alert-triangle.svg"));
	connect(replace_action, &QAction::triggered, this, &StatusBar::replace_requested);

	menu.exec(db_button->mapToGlobal(db_button->rect().bottomLeft()));
}
