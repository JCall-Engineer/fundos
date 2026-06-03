#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSvgWidget>
#include "database_error.hpp"

DatabaseErrorPage::DatabaseErrorPage(const fundos::db::status& status, QWidget* parent) : QWidget(parent) {
	auto* outer = new QVBoxLayout(this);
	outer->setAlignment(Qt::AlignCenter);

	auto* icon = new QSvgWidget(":/icons/database-off.svg", this);
	icon->setFixedSize(48, 48);
	outer->addWidget(icon, 0, Qt::AlignHCenter);

	auto* title = new QLabel(this);
	title->setAlignment(Qt::AlignHCenter);
	outer->addWidget(title);

	auto* description = new QLabel(this);
	description->setAlignment(Qt::AlignHCenter);
	description->setWordWrap(true);
	outer->addWidget(description);

	auto* buttons = new QVBoxLayout();
	buttons->setAlignment(Qt::AlignHCenter);
	outer->addLayout(buttons);

	auto add_button = [&](const QString& label, auto signal) {
		auto* button = new QPushButton(label, this);
		buttons->addWidget(button);
		connect(button, &QPushButton::clicked, this, signal);
	};

	auto setup_error = [&](const QString& title_text, QString description_text) {
		title->setText(title_text);
		if (status.sqlite3_outcome.msg.has_value()) {
			auto view = status.sqlite3_outcome.msg->view();
			description_text += "\n\n" + tr("Error Description: \"") + QString::fromUtf8(view.data(), view.size()) + "\"";
		}
		description->setText(description_text);
	};

	using code = fundos::db::status::code;
	using schema = fundos::db::schema_state;
	using error = fundos::db::error;

	switch (status.result) {
		case code::ok: {
			FUNDOS_UNREACHABLE();
			break;
		}
		case code::needs_migration: {
			setup_error(
				tr("Database Migration Required"),
				tr("This database was created by an older version of FundOS and needs to be migrated.")
			);
			add_button(tr("Backup database..."), &DatabaseErrorPage::backup_requested);
			add_button(tr("Migrate database"), &DatabaseErrorPage::migrate_requested);
			add_button(tr("Quit"), &DatabaseErrorPage::quit_requested);
			break;
		}
		case code::null_db: {
			FUNDOS_UNREACHABLE();
			break;
		}
		case code::schema_error: {
			switch (status.schema_status) {
				case schema::newer_schema: {
					setup_error(
						tr("Database Requires a Newer Version"),
						tr("This database was created by a newer version of FundOS. Update FundOS to access it.")
					);
					add_button(tr("Quit"), &DatabaseErrorPage::quit_requested);
					break;
				}
				case schema::app_mismatch: {
					setup_error(
						tr("Unrecognized Database File"),
						tr("This file was not created by FundOS.")
					);
					add_button(tr("Extract Broken Database..."), &DatabaseErrorPage::backup_requested);
					add_button(tr("Overwrite with New Database"), &DatabaseErrorPage::create_new_requested);
					add_button(tr("Overwrite with External File..."), &DatabaseErrorPage::replace_requested);
					add_button(tr("Quit"), &DatabaseErrorPage::quit_requested);
					break;
				}
				case schema::schema_mismatch: {
					setup_error(
						tr("Database Schema is Corrupted"),
						tr("The database structure is unrecognized and cannot be recovered.")
					);
					add_button(tr("Extract Broken Database..."), &DatabaseErrorPage::backup_requested);
					add_button(tr("Overwrite with New Database"), &DatabaseErrorPage::create_new_requested);
					add_button(tr("Overwrite with External File..."), &DatabaseErrorPage::replace_requested);
					add_button(tr("Quit"), &DatabaseErrorPage::quit_requested);
					break;
				}
				case schema::none:         // immediately should trigger creation
				case schema::current:      // would not cause a schema_error
				case schema::migrated:     // should overwrite the schema_error
				case schema::older_schema: // would have triggered code::needs_migration
					FUNDOS_UNREACHABLE();
					break;
			}
			break;
		}
		case code::sqlite3_error: {
			switch (status.sqlite3_outcome.code) {
				case error::corrupted: {
					setup_error(
						tr("Database is Corrupted"),
						tr("The database file is damaged and cannot be opened.")
					);
					add_button(tr("Extract Broken Database..."), &DatabaseErrorPage::backup_requested);
					add_button(tr("Overwrite with New Database"), &DatabaseErrorPage::create_new_requested);
					add_button(tr("Overwrite with External File..."), &DatabaseErrorPage::replace_requested);
					add_button(tr("Quit"), &DatabaseErrorPage::quit_requested);
					break;
				}
				case error::unavailable: {
					setup_error(
						tr("Database Unavailable"),
						tr("The database is busy or locked by another process.")
					);
					add_button(tr("Try Again"), &DatabaseErrorPage::retry_requested);
					add_button(tr("Extract Broken Database..."), &DatabaseErrorPage::backup_requested);
					add_button(tr("Overwrite with New Database"), &DatabaseErrorPage::create_new_requested);
					add_button(tr("Quit"), &DatabaseErrorPage::quit_requested);
					break;
				}
				case error::readonly: {
					setup_error(
						tr("Database is Read-Only"),
						tr("FundOS does not have permission to write to the database. Check your filesystem permissions.")
					);
					add_button(tr("Try Again"), &DatabaseErrorPage::retry_requested);
					add_button(tr("Quit"), &DatabaseErrorPage::quit_requested);
					break;
				}
				case error::out_of_memory: {
					setup_error(
						tr("Out of Memory"),
						tr("FundOS ran out of memory while opening the database.")
					);
					add_button(tr("Try Again"), &DatabaseErrorPage::retry_requested);
					add_button(tr("Quit"), &DatabaseErrorPage::quit_requested);
					break;
				}
				case error::disk_full: {
					setup_error(
						tr("Disk Full"),
						tr("There is not enough disk space to open the database.")
					);
					add_button(tr("Try Again"), &DatabaseErrorPage::retry_requested);
					add_button(tr("Quit"), &DatabaseErrorPage::quit_requested);
					break;
				}
				case error::none:        // We wouldn't be here if there was no error
				case error::not_ready:   // only returned by query methods
				case error::not_found:   // only returned by query methods
				case error::bad_request: // only returned by query methods
				case error::rejected:    // only returned by query methods
					FUNDOS_UNREACHABLE();
					break;
				case error::internal: {
					setup_error(
						tr("Internal Error"),
						tr("An unexpected internal error occurred. Please report this issue.")
					);
					add_button(tr("Quit"), &DatabaseErrorPage::quit_requested);
					break;
				}
			}
			break;
		}
	}
}
