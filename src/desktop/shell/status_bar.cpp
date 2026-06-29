#include "status_bar.hpp"
#include "theme.hpp"
#include <QHBoxLayout>
#include <QMenu>
#include <QWidgetAction>

StatusBar::StatusBar(QWidget* parent) : QStatusBar(parent) {
	setSizeGripEnabled(false);
	setStyleSheet("QStatusBar::item { border: none; }");

	auto* left = new QWidget(this);
	auto* layout = new QHBoxLayout(left);
	layout->setContentsMargins(8, 0, 8, 4);
	layout->setAlignment(Qt::AlignVCenter);
	layout->setSpacing(6);

	dot = new QWidget(left);
	dot->setFixedSize(10, 10);
	layout->addWidget(dot, 0, Qt::AlignVCenter);

	text = new QLabel(left);
	layout->addWidget(text, 0, Qt::AlignVCenter);

	addWidget(left);

	db_button = new QToolButton(this);
	db_button->setIcon(theme::colored_svg_icon(":/icons/database.svg", theme::text, theme::toolbar_icon_size));
	db_button->setAutoRaise(true);
	db_button->setFixedSize(theme::toolbar_icon_size);
	connect(db_button, &QToolButton::clicked, this, &StatusBar::show_db_menu);
	addPermanentWidget(db_button);
	left->setFixedHeight(db_button->height());
}

void StatusBar::apply_ready() {
	dot->setStyleSheet(QString("background: %1; border-radius: 5px;").arg(theme::success_foreground.name()));
	text->setText(tr("ready"));
}

void StatusBar::apply_yellow(const QString& message) {
	dot->setStyleSheet(QString("background: %1; border-radius: 5px;").arg(theme::warning_foreground.name()));
	text->setText(message);
}

void StatusBar::apply_red(const QString& message) {
	dot->setStyleSheet(QString("background: %1; border-radius: 5px;").arg(theme::error_foreground.name()));
	text->setText(message);
}

void StatusBar::apply_error(const QString& message) {
	if (!is_connected) {
		apply_red(message);
	} else {
		apply_yellow(message);
	}
}

void StatusBar::on_db_open(fundos::db::status open_result) {
	text->setToolTip({});
	is_connected = !open_result.has_error();
	db_button->setEnabled(is_connected);

	using code = fundos::db::status::code;
	using schema = fundos::db::schema_state;
	using error = fundos::db::error;

	switch (open_result.result) {
		case code::ok:
			return apply_ready();
		case code::needs_migration:
			return apply_error(tr("migration required"));
		case code::null_db:
			return apply_error(tr("db error: null"));
		case code::schema_error: {
			switch (open_result.schema_status) {
				case schema::newer_schema:
					return apply_error(tr("database requires a newer version of fundos"));
				case schema::schema_mismatch:
					return apply_error(tr("database schema is corrupted"));
				case schema::app_mismatch:
					return apply_error(tr("unrecognized database file"));
				default:
					return apply_error(tr("schema error code: %1").arg(QString::number(static_cast<int>(open_result.schema_status))));
			}
		}
		case code::sqlite3_error: {
			if (open_result.sqlite3_outcome.msg.has_value()) {
				const auto& view = open_result.sqlite3_outcome.msg->view();
				text->setToolTip(QString::fromUtf8(view.data(), view.size()));
			}
			switch (open_result.sqlite3_outcome.code) {
				case error::corrupted:
					return apply_error(tr("database is corrupted"));
				case error::unavailable:
					return apply_error(tr("database unavailable"));
				case error::inaccessible:
					return apply_error(tr("could not open the database"));
				case error::readonly:
					return apply_error(tr("database is read-only"));
				case error::out_of_memory:
					return apply_error(tr("out of memory"));
				case error::disk_full:
					return apply_error(tr("disk full"));
				case error::internal:
					return apply_error(tr("unexpected internal error"));
				default:
					return apply_error(tr("db error code: %1").arg(QString::number(static_cast<int>(open_result.sqlite3_outcome.code))));
			}
		}
	}
}

void StatusBar::set_status(const fundos::db::outcome& outcome) {
	text->setToolTip({});
	if (outcome.code == fundos::db::error::corrupted || outcome.code == fundos::db::error::internal) {
		is_connected = false;
		db_button->setEnabled(false);
	}

	//: Status bar error; %1 is the error description, shown when database connection is lost
	auto disconnected = [&](const QString& message) {
		return is_connected ? message : tr("disconnected: %1").arg(message);
	};

	if (outcome.msg.has_value()) {
		const auto& view = outcome.msg->view();
		text->setToolTip(QString::fromUtf8(view.data(), view.size()));
	}
	using error = fundos::db::error;
	switch (outcome.code) {
		case error::none:
			return apply_ready();
		case error::not_ready:
			return apply_error(disconnected(tr("database is not ready")));
		case error::corrupted:
			return apply_error(disconnected(tr("database is corrupted")));
		case error::unavailable:
			return apply_error(disconnected(tr("database busy, try again")));
		case error::inaccessible: // would not trigger a db::outcome but a db::status
			FUNDOS_UNREACHABLE();
		case error::readonly:
			return apply_error(disconnected(tr("database is read-only")));
		case error::out_of_memory:
			return apply_error(disconnected(tr("out of memory")));
		case error::disk_full:
			return apply_error(disconnected(tr("disk full")));
		case error::constraint:
			return apply_error(disconnected(tr("constraint violation")));
		case error::not_found:
			return apply_error(disconnected(tr("record not found")));
		case error::bad_request:
			return apply_error(disconnected(tr("incorrect API usage")));
		case error::rejected:
			return apply_error(disconnected(tr("attempted illegal operation")));
		case error::internal:
			return apply_error(disconnected(tr("unexpected internal error")));
		case error::interrupted:
			return apply_ready(); // interrupts are user-initiated; treat as a clean state
	}
}

static QString format_size(int64_t bytes) {
	// Ordered largest to smallest; first match wins.
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
	auto* menu = new QMenu(this);
	auto add_section_header = [&](const QString& title) {
		auto* widget = new QWidget(menu);
		auto* layout = new QHBoxLayout(widget);
		layout->setContentsMargins(8, 6, 16, 2);

		auto* label = new QLabel(title, widget);
		QFont font = label->font();
		font.setPointSizeF(font.pointSizeF() * 0.8);
		label->setFont(font);
		label->setStyleSheet("color: " + theme::text_muted.name() + ";");

		layout->addWidget(label);

		auto* action = new QWidgetAction(menu);
		action->setDefaultWidget(widget);
		menu->addAction(action);
	};
	auto add_info_row = [&](const QString& key, QLabel** value_label) {
		auto* widget = new QWidget(menu);
		auto* layout = new QHBoxLayout(widget);
		layout->setContentsMargins(16, 4, 16, 4);

		auto* key_label = new QLabel(key, widget);
		*value_label = new QLabel(widget);

		//: Minimum width is derived from the longest value we expect to display (a size like "123.4 MB").
		//: The label starts empty; on_db_info fills it once the database thread responds.
		(*value_label)->setMinimumWidth((*value_label)->fontMetrics().horizontalAdvance(tr("123.4 MB")));
		(*value_label)->setAlignment(Qt::AlignRight);

		layout->addWidget(key_label);
		layout->addWidget(*value_label);

		auto* action = new QWidgetAction(menu);
		action->setDefaultWidget(widget);
		menu->addAction(action);
	};

	// DATABASE section
	add_section_header(tr("DATABASE"));
	add_info_row(tr("size on disk"),   &info_size);
	add_info_row(tr("journal mode"),   &info_journal);
	add_info_row(tr("schema version"), &info_schema);

	// ACTIONS section
	add_section_header(tr("ACTIONS"));
	connect(menu->addAction(tr("Manage Locale...")), &QAction::triggered, this, &StatusBar::manage_locale_requested);
	connect(menu->addAction(tr("Back Up Database...")), &QAction::triggered, this, &StatusBar::backup_requested);

	menu->addSeparator();
	auto* restore_action = menu->addAction(tr("Restore from Backup..."));
	restore_action->setIcon(theme::colored_svg_icon(":/icons/alert-triangle.svg", theme::warning_foreground, theme::toolbar_icon_size));
	connect(restore_action, &QAction::triggered, this, &StatusBar::restore_requested);

	auto* replace_action = menu->addAction(tr("Replace with New Database..."));
	replace_action->setIcon(theme::colored_svg_icon(":/icons/alert-triangle.svg", theme::warning_foreground, theme::toolbar_icon_size));
	connect(replace_action, &QAction::triggered, this, &StatusBar::create_new_requested);

	connect(menu, &QMenu::aboutToHide, this, [this]() {
		info_size    = nullptr;
		info_journal = nullptr;
		info_schema  = nullptr;
	});

	emit db_info_requested();
	menu->adjustSize();

	// Position menu so its bottom-right corner aligns with the button's top-right corner.
	menu->exec(db_button->mapToGlobal(
		db_button->rect().topRight() - QPoint(menu->width(), menu->height())
	));
}

void StatusBar::on_db_info(AppDatabase::DatabaseInfo info) {
	if (info_size)    { info_size->setText(format_size(info.size_on_disk)); }
	if (info_journal) { info_journal->setText(QString::fromStdString(info.journal_mode)); }
	if (info_schema)  { info_schema->setText(QString::number(info.schema_version)); }
}
