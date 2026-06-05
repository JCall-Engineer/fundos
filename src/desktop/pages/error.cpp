#include "error.hpp"
#include "theme.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSvgWidget>

void ErrorPage::setup_layout() {
	auto* outer = new QVBoxLayout(this);
	outer->setAlignment(Qt::AlignCenter);
	outer->setContentsMargins(0, 0, 0, 0);

	auto* card = new QWidget(this);
	card->setMinimumWidth(280);
	outer->addWidget(card, 0, Qt::AlignHCenter);

	auto* layout = new QVBoxLayout(card);
	layout->setSpacing(8);

	auto* icon_label = new QLabel(card);
	icon_label->setPixmap(theme::colored_svg(":/icons/database-off.svg", theme::error_foreground, QSize(48, 48)));
	layout->addWidget(icon_label, 0, Qt::AlignHCenter);

	title = new QLabel(card);
	title->setAlignment(Qt::AlignHCenter);
	QFont title_font = title->font();
	title_font.setPointSizeF(title_font.pointSizeF() * 1.3);
	title_font.setBold(true);
	title->setFont(title_font);
	layout->addWidget(title);

	description = new QLabel(card);
	description->setAlignment(Qt::AlignHCenter);
	description->setWordWrap(true);
	layout->addWidget(description);

	layout->addSpacing(8);

	buttons = new QVBoxLayout();
	buttons->setSpacing(6);
	layout->addLayout(buttons);
}

ErrorPage::button_spec ErrorPage::resolve_button(button which) {
	switch (which) {
		case button::retry:      return { tr("Try Again"),                    &ErrorPage::retry_requested      };
		case button::migrate:    return { tr("Migrate Database"),             &ErrorPage::migrate_requested    };
		case button::backup:     return { tr("Backup Existing Database"),     &ErrorPage::backup_requested     };
		case button::create_new: return { tr("Overwrite with New Database"),  &ErrorPage::create_new_requested };
		case button::restore:    return { tr("Restore Database from Backup"), &ErrorPage::restore_requested    };
		case button::quit:       return { tr("Quit"),                         &ErrorPage::quit_requested       };
	}
	FUNDOS_UNREACHABLE();
}

void ErrorPage::add_button(button which) {
	const auto spec = resolve_button(which);
	auto* b = new QPushButton(spec.label, this);
	buttons->addWidget(b);
	connect(b, &QPushButton::clicked, this, spec.signal);
}

void ErrorPage::setup_error(const QString& title_text, QString description_text, const std::optional<fundos::db::message>& msg) {
	title->setText(title_text);
	if (msg.has_value()) {
		auto view = msg->view();
		description_text += "\n\n" + tr("Error Message:") + " \"" + QString::fromUtf8(view.data(), view.size()) + "\"";
	}
	description->setText(description_text);
};

ErrorPage::ErrorPage(const fundos::db::status& status, QWidget* parent) : QWidget(parent) {
	setup_layout();

	auto setup_open_error = [&](const QString& title_text, QString description_text) {
		setup_error(title_text, description_text, status.sqlite3_outcome.msg);
	};

	using code = fundos::db::status::code;
	using schema = fundos::db::schema_state;
	using error = fundos::db::error;

	// A rule about buttons: If recovery options are present they must be preceded by a backup button
	switch (status.result) {
		case code::ok: {
			FUNDOS_UNREACHABLE();
			break;
		}
		case code::needs_migration: {
			// The database is fine, recovery options are not appropriate, but backup before migration is
			setup_open_error(
				tr("Database Migration Required"),
				tr("This database was created by an older version of FundOS and needs to be migrated.") + " " +
				tr("It is recommended you make a backup before proceeding.")
			);
			add_button(button::backup);
			add_button(button::migrate);
			add_button(button::quit);
			break;
		}
		case code::null_db: {
			FUNDOS_UNREACHABLE();
			break;
		}
		case code::schema_error: {
			// FundOS cannot use this database in any way: offer recovery options
			switch (status.schema_status) {
				case schema::newer_schema: {
					setup_open_error(
						tr("Database Requires a Newer Version"),
						tr("This database was created by a newer version of FundOS. Update FundOS to access it.")
					);
					add_button(button::backup);
					add_button(button::create_new);
					add_button(button::restore);
					add_button(button::quit);
					break;
				}
				case schema::app_mismatch: {
					setup_open_error(
						tr("Unrecognized Database File"),
						tr("This file was not created by FundOS.")
					);
					add_button(button::backup);
					add_button(button::create_new);
					add_button(button::restore);
					add_button(button::quit);
					break;
				}
				case schema::schema_mismatch: {
					setup_open_error(
						tr("Database Schema is Corrupted"),
						tr("The database structure is unrecognized and cannot be recovered.")
					);
					add_button(button::backup);
					add_button(button::create_new);
					add_button(button::restore);
					add_button(button::quit);
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
					// The database is broken: offer recovery options
					setup_open_error(
						tr("Database is Corrupted"),
						tr("The database file is damaged and cannot be opened.")
					);
					add_button(button::backup);
					add_button(button::create_new);
					add_button(button::restore);
					add_button(button::quit);
					break;
				}
				case error::unavailable: {
					// We cannot write to the database right now, which is required for recovery options: offer try again
					setup_open_error(
						tr("Database Unavailable"),
						tr("The database is busy or locked by another process.")
					);
					add_button(button::retry);
					add_button(button::quit);
					break;
				}
				case error::inaccessible: {
					// We do not have write access, which is required for recovery options: offer try again
					setup_open_error(
						tr("Can't Open Database"),
						tr("The path to the database does not exist or FundOS does not have permission to access it.")
					);
					add_button(button::retry);
					add_button(button::quit);
					break;
				}
				case error::readonly: {
					// We do not have write access, which is required for recovery options: offer try again
					setup_open_error(
						tr("Database is Read-Only"),
						tr("FundOS does not have permission to write to the database. Check your filesystem permissions.")
					);
					add_button(button::retry);
					add_button(button::quit);
					break;
				}
				case error::out_of_memory: {
					// The system is out of resources, nothing can be done from our end: offer try again
					setup_open_error(
						tr("Out of Memory"),
						tr("FundOS ran out of memory while opening the database.")
					);
					add_button(button::retry);
					add_button(button::quit);
					break;
				}
				case error::disk_full: {
					// The system is out of resources, nothing can be done from our end: offer try again
					setup_open_error(
						tr("Disk Full"),
						tr("There is not enough disk space to open the database.")
					);
					add_button(button::retry);
					add_button(button::quit);
					break;
				}
				case error::none:        // We wouldn't be here if there was no error
				case error::constraint:  // Genuinely impossible during db::open()
				case error::not_ready:   // only returned by query methods
				case error::not_found:   // only returned by query methods
				case error::bad_request: // only returned by query methods
				case error::rejected:    // only returned by query methods
					FUNDOS_UNREACHABLE();
					break;
				case error::internal: {
					// Something really bad happened and the system is in an unpredictable state: no reasonable action can be taken
					setup_open_error(
						tr("Internal Error"),
						tr("An unexpected internal error occurred.") + " " + tr("Please report this issue.")
					);
					add_button(button::quit);
					break;
				}
			}
			break;
		}
	}
}

ErrorPage::ErrorPage(const fundos::db::outcome& outcome, QWidget* parent) : QWidget(parent) {
	using error = fundos::db::error;
	setup_layout();
	switch (outcome.code) {
		case error::corrupted:
			// In the runtime case a transient io error is possible here
			setup_error(
				tr("Storage Error"),
				tr("The database encountered an unrecoverable error."),
				outcome.msg
			);
			add_button(button::retry);
			add_button(button::backup);
			add_button(button::create_new);
			add_button(button::restore);
			add_button(button::quit);
			break;

		case error::internal:
			setup_error(
				tr("Internal Error"),
				tr("An unexpected internal error occurred.") + " " + tr("Please report this issue."),
				outcome.msg
			);
			add_button(button::retry);
			add_button(button::quit);
			break;

		default:
			// The only errors that result in closing the db are corrupted or internal. Verify with db::sqlite_runtime_error and MainWindow::on_result
			FUNDOS_UNREACHABLE();
	}
}
