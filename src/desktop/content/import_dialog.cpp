#include "import_dialog.hpp"
#include "theme.hpp"
#include "components/loading_spinner.hpp"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QtConcurrent>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSettings>
#include <QTimer>

QString ImportDialog::warning_message(fundos::import::warning type, int32_t count) {
	using warning = fundos::import::warning;
	switch (type) {
		case warning::missing_acctid:      return tr("%n accounts(s) were missing an unique identifier and could not be imported.", "", count);
		case warning::skipped_transaction: return tr("%n transaction(s) could not be parsed and were skipped.", "", count);
		case warning::missing_fitid:       return tr("%n transaction(s) were missing a unique identifier and may be imported as duplicates.", "", count);
		case warning::missing_date:        return tr("%n transaction(s) were missing a date.", "", count);
		case warning::missing_amount:      return tr("%n transaction(s) were missing an amount.", "", count);
		case warning::bad_date:            return tr("%n transaction(s) had a date that could not be parsed.", "", count);
		case warning::bad_amount:          return tr("%n transaction(s) had an amount that could not be parsed.", "", count);
		case warning::bad_correction:      return tr("%n transaction(s) declared an unkown correct action.", "", count);
		case warning::NUM_WARNINGS:        return QString();
	}
	FUNDOS_UNREACHABLE();
}

ImportDialog::ImportDialog(AppCoordinator* coordinator, QWidget* parent) : QDialog(parent), app_coordinator(coordinator) {
	setWindowTitle(tr("Import from Bank"));
	setMinimumWidth(600);

	layout = new QVBoxLayout(this);

	auto* db = app_coordinator->database();
	connect(this, &ImportDialog::save_account_requested,     db,   &AppDatabase::save_account);
	connect(this, &ImportDialog::refresh_accounts_requested, db,   &AppDatabase::request_accounts);
	connect(this, &ImportDialog::prepare_import_requested,   db,   &AppDatabase::prepare_import);
	connect(this, &ImportDialog::perform_import_requested,   db,   &AppDatabase::perform_import);
	connect(db,   &AppDatabase::account_saved,               this, &ImportDialog::on_account_saved);
	connect(app_coordinator, &AppCoordinator::refreshed,     this, &ImportDialog::on_accounts_updated);
	connect(db,   &AppDatabase::import_prepared,             this, &ImportDialog::on_import_prepared);
	connect(db,   &AppDatabase::import_performed,            this, &ImportDialog::on_import_performed);

	show_file_selection_page();
}

void ImportDialog::show_file_selection_page() {
	auto* page   = new QWidget(this);
	auto* vbox   = new QVBoxLayout(page);
	auto* label  = new QLabel(tr("Select an OFX file to import:"), page);
	auto* row    = new QHBoxLayout();
	auto* path   = new QLineEdit(page);
	auto* browse = new QPushButton(tr("Browse..."), page);
	auto* next   = new QPushButton(tr("Next"), page);
	auto* cancel = new QPushButton(tr("Cancel"), page);

	path->setReadOnly(true);
	next->setEnabled(false);

	auto* button_row = new QHBoxLayout();
	button_row->addStretch();
	button_row->addWidget(cancel);
	button_row->addWidget(next);

	row->addWidget(path);
	row->addWidget(browse);
	vbox->addWidget(label);
	vbox->addLayout(row);
	vbox->addStretch();
	vbox->addLayout(button_row);

	connect(cancel, &QPushButton::clicked, this, &ImportDialog::reject);
	connect(browse, &QPushButton::clicked, this, [this, path, next]() {
		QSettings settings;
		QString last_directory = settings.value("import/last_directory", QDir::homePath()).toString();
		QString selected = QFileDialog::getOpenFileName(
			this,
			tr("Import from Bank"),
			last_directory,
			tr("OFX File (*.ofx)")
		);
		if (selected.isEmpty()) { return; }
		settings.setValue("import/last_directory", QFileInfo(selected).absolutePath());
		path->setText(selected);
		next->setEnabled(true);
	});
	connect(next, &QPushButton::clicked, this, [this, path]() {
		std::string filepath = path->text().toStdString();
		fundos::currency_locale::spec locale = app_coordinator->context()->currency_locale().info();
		parse_watcher = new QFutureWatcher<fundos::import::result>(this);
		connect(parse_watcher, &QFutureWatcher<fundos::import::result>::finished, this, &ImportDialog::on_parse_finished);
		show_spinner_page(tr("Reading file...")); // path is an invalid pointer at this point
		QTimer::singleShot(0, this, [this, filepath, locale]() {
			parse_watcher->setFuture(QtConcurrent::run([filepath, locale]() {
				return fundos::import::import_ofx(filepath, locale);
			}));
		});
	});

	while (QLayoutItem* item = layout->takeAt(0)) {
		delete item->widget();
		delete item;
	}
	layout->addWidget(page);
	adjustSize();
}

void ImportDialog::show_spinner_page(const QString& description) {
	auto* page    = new QWidget(this);
	auto* vbox    = new QVBoxLayout(page);
	auto* row     = new QHBoxLayout();
	auto* spinner = new LoadingSpinner(page);
	auto* label   = theme::header_label(description, page);

	vbox->addStretch();
	row->addStretch();
	row->addWidget(spinner);
	row->addWidget(label);
	row->addStretch();
	vbox->addLayout(row);
	vbox->addStretch();

	while (QLayoutItem* item = layout->takeAt(0)) {
		delete item->widget();
		delete item;
	}
	layout->addWidget(page);
	adjustSize();
}

void ImportDialog::show_parse_error_page(fundos::import::error error) {
	using parse_error = fundos::import::error;

	auto* page   = new QWidget(this);
	auto* vbox   = new QVBoxLayout(page);
	auto* detail = new QLabel(page);
	auto* close  = new QPushButton(tr("Close"), page);

	switch (error) {
		case parse_error::bad_format: detail->setText(tr("The selected file is not a recognized OFX format."));  break;
		case parse_error::io_error:   detail->setText(tr("There was an error reading the selected file."));      break;
		case parse_error::malformed:  detail->setText(tr("The selected file is damaged or incomplete."));        break;
		case parse_error::none:       FUNDOS_UNREACHABLE();                                                      break;
	}

	auto* button_row = new QHBoxLayout();
	button_row->addStretch();
	button_row->addWidget(close);

	detail->setAlignment(Qt::AlignCenter);
	detail->setWordWrap(true);

	vbox->addStretch();
	vbox->addWidget(detail);
	vbox->addStretch();
	vbox->addLayout(button_row);

	connect(close, &QPushButton::clicked, this, &ImportDialog::reject);

	while (QLayoutItem* item = layout->takeAt(0)) {
		delete item->widget();
		delete item;
	}
	layout->addWidget(page);
	adjustSize();
}

void ImportDialog::show_warnings_page(const fundos::import::result& result) {
	auto* page   = new QWidget(this);
	auto* vbox   = new QVBoxLayout(page);
	auto* header = new QLabel(tr("The file was read with the following warnings:"), page);

	vbox->addWidget(header);

	using warning = fundos::import::warning;
	for (int32_t index = 0; index < static_cast<int32_t>(warning::NUM_WARNINGS); ++index) {
		int32_t count = result.warning_counts[index];
		if (count > 0) {
			vbox->addWidget(new QLabel(warning_message(static_cast<warning>(index), count), page));
		}
	}

	vbox->addStretch();

	auto* button_row = new QHBoxLayout();
	auto* cancel     = new QPushButton(tr("Cancel"), page);
	auto* next       = new QPushButton(tr("Continue"), page);
	button_row->addStretch();
	button_row->addWidget(cancel);
	button_row->addWidget(next);
	vbox->addLayout(button_row);

	connect(cancel, &QPushButton::clicked, this, &ImportDialog::reject);
	connect(next,   &QPushButton::clicked, this, &ImportDialog::on_accounts_updated);

	while (QLayoutItem* item = layout->takeAt(0)) {
		delete item->widget();
		delete item;
	}
	layout->addWidget(page);
	adjustSize();
}

void ImportDialog::show_account_page(fundos::import::bank_account* bank_account) {
	auto* page    = new QWidget(this);
	auto* vbox    = new QVBoxLayout(page);
	auto* balance = new QLabel(tr("Balance: %1 as of %2")
		.arg(QString::fromStdString(bank_account->balance.to_string(app_coordinator->context()->currency_locale().info())))
		.arg(QDateTime::fromMSecsSinceEpoch(bank_account->as_of.milliseconds_since_epoch).toString(QLocale::system().dateFormat(QLocale::ShortFormat))),
		page);

	auto* preview = new QListWidget(page);
	preview->setFixedHeight(150);
	for (const auto& imported : bank_account->transactions) {
		preview->addItem(tr("%1 — %2")
			.arg(QString::fromStdString(imported.record.memo))
			.arg(QString::fromStdString(imported.record.amount.to_string(app_coordinator->context()->currency_locale().info()))));
	}

	auto* prompt = new QLabel(tr("Assign bank account \"%1\" to:").arg(QString::fromStdString(bank_account->acct_id)), page);

	auto* picker = new QComboBox(page);
	picker->addItem(tr("Create New Account"), QVariant());
	for (const auto& account : app_coordinator->context()->accounts()) {
		if (account.bank_account_id.has_value()) { continue; }
		picker->addItem(QString::fromStdString(account.name), QVariant::fromValue(account.id()));
	}

	auto* confirm    = new QPushButton(tr("Continue"), page);
	auto* cancel     = new QPushButton(tr("Cancel"), page);
	auto* button_row = new QHBoxLayout();
	button_row->addStretch();
	button_row->addWidget(cancel);
	button_row->addWidget(confirm);

	vbox->addWidget(balance);
	vbox->addWidget(preview);
	vbox->addSpacing(16);
	vbox->addWidget(prompt);
	vbox->addWidget(picker);
	vbox->addStretch();
	vbox->addLayout(button_row);

	connect(cancel, &QPushButton::clicked, this, &ImportDialog::reject);
	connect(confirm, &QPushButton::clicked, this, [this, picker, bank_account, confirm]() {
		if (!picker->currentData().isValid()) {
			bool accepted = false;
			QString name = QInputDialog::getText(this, tr("New Account"), tr("Account name:"), QLineEdit::Normal, "", &accepted);
			if (!accepted) { return; }
			name = name.trimmed();
			if (name.isEmpty()) { return; }
			fundos::account creating = {
				.name            = name.toStdString(),
				.bank_account_id = bank_account->acct_id,
			};
			confirm->setEnabled(false);
			emit save_account_requested(creating);
		} else {
			int64_t account_id = picker->currentData().value<int64_t>();
			auto* existing = app_coordinator->context()->account(account_id);
			if (existing == nullptr) { return; }
			fundos::account updating = *existing;
			updating.bank_account_id = bank_account->acct_id;
			confirm->setEnabled(false);
			emit save_account_requested(updating);
		}
	});

	while (QLayoutItem* item = layout->takeAt(0)) {
		delete item->widget();
		delete item;
	}
	layout->addWidget(page);
	adjustSize();
}

void ImportDialog::show_transaction_page() {
	auto* page          = new QWidget(this);
	auto* vbox          = new QVBoxLayout(page);
	auto* bulk_row      = new QHBoxLayout();
	auto* use_existing  = new QPushButton(tr("Use All Existing Memos"), page);
	auto* use_imported  = new QPushButton(tr("Use All Imported Memos"), page);
	auto* show_all      = new QCheckBox(tr("Show all transactions"), page);
	auto* list_header   = theme::header_label(tr("Transactions with potential matches in your register:"), page);
	auto* scroll        = new QScrollArea(page);
	auto* scroll_widget = new QWidget(scroll);
	auto* card_layout   = new QVBoxLayout(scroll_widget);
	auto* button_row    = new QHBoxLayout();
	auto* cancel        = new QPushButton(tr("Cancel"), page);
	auto* finish        = new QPushButton(tr("Finish"), page);

	bulk_row->addWidget(use_existing);
	bulk_row->addWidget(use_imported);
	bulk_row->addStretch();
	bulk_row->addWidget(show_all);

	scroll->setMinimumHeight(400);
	scroll->setWidget(scroll_widget);
	scroll->setWidgetResizable(true);
	card_layout->setAlignment(Qt::AlignTop);

	button_row->addStretch();
	button_row->addWidget(cancel);
	button_row->addWidget(finish);

	using memo_choice = fundos::import::imported_transaction::memo_choice;

	struct CardWidgets {
		QFrame*       card;
		QWidget*      existing_row;
		QLabel*       existing_date;
		QLabel*       existing_memo;
		QRadioButton* radio_existing;
		QRadioButton* radio_importing;
		QWidget*      memo_row;
		bool          has_decision;
	};

	auto all_cards = std::make_shared<std::vector<CardWidgets>>();
	size_t count_transactions = 0;
	size_t count_definitive = 0;
	size_t count_matchable = 0;
	size_t count_new = 0;

	page->setUpdatesEnabled(false);
	for (auto& bank_account : importing->accounts) {
		count_transactions += bank_account.transactions.size();
		for (auto& txn : bank_account.transactions) {
			const bool has_match      = txn.get_match() != nullptr;
			const bool is_definitive  = txn.is_definitive_match();
			const bool can_match      = bank_account.has_any_candidates(txn);
			const bool has_decision   = is_definitive || can_match;

			auto* card        = new QFrame(scroll_widget);
			auto* card_vbox   = new QVBoxLayout(card);
			card->setFrameShape(QFrame::StyledPanel);

			// Middle section
			auto* middle      = new QWidget(card);
			auto* middle_vbox = new QVBoxLayout(middle);
			middle_vbox->setContentsMargins(0, 0, 0, 0);

			auto* existing_row  = new QWidget(middle);
			auto* existing_hbox = new QHBoxLayout(existing_row);
			auto* existing_label = new QLabel(tr("Existing"), existing_row);
			auto* existing_date  = new QLabel(existing_row);
			auto* existing_memo  = new QLabel(existing_row);
			existing_hbox->addWidget(existing_label);
			existing_hbox->addWidget(existing_date);
			existing_hbox->addWidget(existing_memo);
			existing_hbox->addStretch();
			existing_hbox->setContentsMargins(0, 0, 0, 0);

			auto* importing_hbox = new QHBoxLayout();
			auto* importing_label = new QLabel(tr("Importing"), card);
			auto* importing_date  = new QLabel(
				txn.record.date_cleared
					? QDateTime::fromMSecsSinceEpoch(txn.record.date_cleared->milliseconds_since_epoch).toString(QLocale::system().dateFormat(QLocale::ShortFormat))
					: tr("—"),
				card
			);
			auto* importing_memo  = new QLabel(QString::fromStdString(txn.record.memo), card);
			importing_memo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
			importing_memo->setWordWrap(false);
			importing_hbox->addWidget(importing_label);
			importing_hbox->addWidget(importing_date);
			importing_hbox->addWidget(importing_memo);
			importing_hbox->addStretch();

			if (has_match) {
				existing_date->setText(QDateTime::fromMSecsSinceEpoch(txn.get_match()->date_recorded.milliseconds_since_epoch).toString(QLocale::system().dateFormat(QLocale::ShortFormat)));
				existing_memo->setText(QString::fromStdString(txn.get_match()->memo));
				existing_row->setVisible(true);
			} else {
				existing_row->setVisible(false);
			}

			middle_vbox->addWidget(existing_row);
			middle_vbox->addLayout(importing_hbox);

			// Footer row
			auto* footer_row    = new QHBoxLayout();
			auto* amount_label  = new QLabel(
				tr("Amount: %1").arg(QString::fromStdString(txn.record.amount.to_string(app_coordinator->context()->currency_locale().info()))),
				card
			);
			auto* memo_widget   = new QWidget(card);
			auto* memo_hbox     = new QHBoxLayout(memo_widget);
			auto* memo_label    = new QLabel(tr("Use Memo"), memo_widget);
			auto* radio_existing  = new QRadioButton(tr("Existing"), memo_widget);
			auto* radio_importing = new QRadioButton(tr("Importing"), memo_widget);
			auto* button_group  = new QButtonGroup(memo_widget);

			button_group->addButton(radio_existing,  static_cast<int>(memo_choice::prefer_existing));
			button_group->addButton(radio_importing, static_cast<int>(memo_choice::prefer_importing));

			(txn.memo == memo_choice::prefer_existing ? radio_existing : radio_importing)->setChecked(true);
			memo_widget->setVisible(has_match);

			memo_hbox->addWidget(memo_label);
			memo_hbox->addWidget(radio_existing);
			memo_hbox->addWidget(radio_importing);
			memo_hbox->setContentsMargins(0, 0, 0, 0);

			connect(button_group, &QButtonGroup::idClicked, this, [&txn](int id) {
				txn.memo = static_cast<memo_choice>(id);
			});

			footer_row->addWidget(amount_label);
			footer_row->addStretch();
			footer_row->addWidget(memo_widget);

			// Header row (saved for last so that the match button can reference other objects)
			auto* header_row  = new QHBoxLayout();
			if (is_definitive) {
				++count_definitive;
				auto* definitive = new QLabel(tr("Already Imported"), card);
				header_row->addWidget(definitive);
			} else if (!can_match) {
				++count_new;
				auto* creating = new QLabel(tr("Importing as New Transaction"), card);
				header_row->addWidget(creating);
			} else {
				++count_matchable;
				auto* match_button = new QPushButton(has_match ? tr("Change Match") : tr("Assign Match"), card);
				header_row->addWidget(match_button);

				connect(match_button, &QPushButton::clicked, this, [this, &txn, &bank_account, match_button, existing_row, existing_date, existing_memo, memo_widget]() {
					auto* picker        = new QDialog(this);
					auto* picker_vbox   = new QVBoxLayout(picker);
					auto* picker_list   = new QListWidget(picker);
					auto* picker_buttons = new QHBoxLayout();
					auto* clear         = new QPushButton(tr("Clear Match"), picker);
					auto* select        = new QPushButton(tr("Select"), picker);

					picker->setWindowTitle(tr("Assign Match"));
					select->setEnabled(false);

					auto candidates = bank_account.valid_candidates_for(txn);
					for (const auto* candidate : candidates) {
						QString label = tr("%1  %2")
							.arg(QDateTime::fromMSecsSinceEpoch(candidate->date_recorded.milliseconds_since_epoch).toString(QLocale::system().dateFormat(QLocale::ShortFormat)))
							.arg(QString::fromStdString(candidate->memo));
						auto* item = new QListWidgetItem(label, picker_list);
						item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(candidate)));
					}

					connect(picker_list, &QListWidget::itemSelectionChanged, this, [picker_list, select]() {
						select->setEnabled(!picker_list->selectedItems().isEmpty());
					});
					connect(clear, &QPushButton::clicked, picker, [picker, &txn, match_button, existing_row, memo_widget]() {
						txn.set_match(nullptr);
						match_button->setText(tr("Assign Match"));
						existing_row->setVisible(false);
						memo_widget->setVisible(false);
						picker->accept();
					});
					connect(select, &QPushButton::clicked, picker, [picker, picker_list, &txn, match_button, existing_row, existing_date, existing_memo, memo_widget]() {
						auto* item = picker_list->currentItem();
						if (!item) { return; }
						const auto* candidate = reinterpret_cast<const fundos::transaction*>(item->data(Qt::UserRole).value<quintptr>());
						txn.set_match(candidate);
						match_button->setText(tr("Change Match"));
						existing_date->setText(QDateTime::fromMSecsSinceEpoch(candidate->date_recorded.milliseconds_since_epoch).toString(QLocale::system().dateFormat(QLocale::ShortFormat)));
						existing_memo->setText(QString::fromStdString(candidate->memo));
						existing_row->setVisible(true);
						memo_widget->setVisible(true);
						picker->accept();
					});

					picker_buttons->addWidget(clear);
					picker_buttons->addStretch();
					picker_buttons->addWidget(select);
					picker_vbox->addWidget(picker_list);
					picker_vbox->addLayout(picker_buttons);

					picker->setAttribute(Qt::WA_DeleteOnClose);
					picker->exec();
				});
			}
			header_row->addStretch();

			card_vbox->addLayout(header_row);
			card_vbox->addWidget(middle);
			card_vbox->addLayout(footer_row);

			all_cards->push_back(CardWidgets{
				.card            = card,
				.existing_row    = existing_row,
				.existing_date   = existing_date,
				.existing_memo   = existing_memo,
				.radio_existing  = radio_existing,
				.radio_importing = radio_importing,
				.memo_row        = memo_widget,
				.has_decision    = has_decision,
			});

			card->setVisible(has_decision);
			card_layout->addWidget(card);
		}
	}

	auto* summary = new QWidget(page);
	auto* summary_layout = new QHBoxLayout(summary);
	summary_layout->setContentsMargins(0, 8, 0, 8);
	summary_layout->addWidget(new QLabel(tr("%1 transactions").arg(count_transactions), summary));
	summary_layout->addWidget(new QLabel(tr("%1 already imported").arg(count_definitive), summary));
	summary_layout->addWidget(new QLabel(tr("%1 potential merges").arg(count_matchable), summary));
	summary_layout->addWidget(new QLabel(tr("%1 importing as new").arg(count_new), summary));
	summary_layout->addStretch();

	vbox->addLayout(bulk_row);
	vbox->addWidget(list_header);
	vbox->addWidget(scroll);
	vbox->addWidget(summary);
	vbox->addLayout(button_row);
	page->setUpdatesEnabled(true);

	connect(use_existing, &QPushButton::clicked, this, [this, all_cards]() {
		for (auto& bank_account : importing->accounts) {
			for (auto& txn : bank_account.transactions) {
				if (txn.get_match() != nullptr) {
					txn.memo = memo_choice::prefer_existing;
				}
			}
		}
		for (auto& widgets : *all_cards) {
			widgets.radio_existing->setChecked(true);
		}
	});

	connect(use_imported, &QPushButton::clicked, this, [this, all_cards]() {
		for (auto& bank_account : importing->accounts) {
			for (auto& txn : bank_account.transactions) {
				txn.memo = memo_choice::prefer_importing;
			}
		}
		for (auto& widgets : *all_cards) {
			widgets.radio_importing->setChecked(true);
		}
	});

	connect(show_all, &QCheckBox::toggled, this, [all_cards](bool checked) {
		for (auto& widgets : *all_cards) {
			if (!widgets.has_decision) {
				widgets.card->setVisible(checked);
			}
		}
	});

	connect(finish, &QPushButton::clicked, this, [this]() {
		show_spinner_page(tr("Importing transactions..."));
		QTimer::singleShot(0, this, [this]() {
			emit perform_import_requested(importing);
		});
	});

	connect(cancel, &QPushButton::clicked, this, &ImportDialog::reject);

	while (QLayoutItem* item = layout->takeAt(0)) {
		delete item->widget();
		delete item;
	}
	layout->addWidget(page);
	adjustSize();
}

void ImportDialog::show_success_page(int32_t imported, int32_t merged) {
	auto* page   = new QWidget(this);
	auto* label  = new QLabel(tr("%1 transaction(s) imported, %2 merged.").arg(imported).arg(merged), page);
	auto* close  = new QPushButton(tr("Close"), page);

	auto* button_row = new QHBoxLayout();
	button_row->addStretch();
	button_row->addWidget(close);

	auto* vbox = new QVBoxLayout(page);
	vbox->addStretch();
	vbox->addWidget(label);
	vbox->addStretch();
	vbox->addLayout(button_row);

	label->setAlignment(Qt::AlignCenter);

	connect(close, &QPushButton::clicked, this, &ImportDialog::accept);

	while (QLayoutItem* item = layout->takeAt(0)) {
		delete item->widget();
		delete item;
	}
	layout->addWidget(page);
	adjustSize();
}

void ImportDialog::on_parse_finished() {
	fundos::import::result result = parse_watcher->result();
	parse_watcher->deleteLater();
	parse_watcher = nullptr;

	if (!result.ok()) {
		show_parse_error_page(result.err);
		return;
	}

	bool has_warnings = false;
	using warning = fundos::import::warning;
	for (int32_t index = 0; index < static_cast<int32_t>(warning::NUM_WARNINGS); ++index) {
		if (result.warning_counts[index] > 0) {
			has_warnings = true;
			break;
		}
	}

	importing = std::make_shared<fundos::import::pending_import>(std::move(result.data));

	if (has_warnings) {
		show_warnings_page(result);
		return;
	}

	on_accounts_updated();
}

void ImportDialog::on_account_saved(fundos::db::outcome status) {
	if (!status) { on_accounts_updated(); return; }
	emit refresh_accounts_requested();
}

void ImportDialog::on_accounts_updated() {
	auto* bank_account = account_iterator.advance_to_unmatched(*importing, app_coordinator->context()->accounts());
	if (bank_account != nullptr) {
		show_account_page(bank_account);
	} else {
		show_spinner_page(tr("Preparing import..."));
		QTimer::singleShot(0, this, [this]() {
			emit prepare_import_requested(importing);
		});
	}
}

void ImportDialog::on_import_prepared(fundos::db::outcome result) {
	if (!result) {
		auto* page   = new QWidget(this);
		auto* vbox   = new QVBoxLayout(page);
		auto* detail = new QLabel(tr("FundOS could not prepare the import. Please try again."), page);
		auto* close  = new QPushButton(tr("Close"), page);

		auto* button_row = new QHBoxLayout();
		button_row->addStretch();
		button_row->addWidget(close);

		detail->setAlignment(Qt::AlignCenter);
		detail->setWordWrap(true);

		vbox->addStretch();
		vbox->addWidget(detail);
		vbox->addStretch();
		vbox->addLayout(button_row);

		connect(close, &QPushButton::clicked, this, &ImportDialog::reject);

		while (QLayoutItem* item = layout->takeAt(0)) {
			delete item->widget();
			delete item;
		}
		layout->addWidget(page);
		adjustSize();
		return;
	}
	show_transaction_page();
}

void ImportDialog::on_import_performed(fundos::db::outcome result) {
	if (!result) {
		auto* page   = new QWidget(this);
		auto* vbox   = new QVBoxLayout(page);
		auto* detail = new QLabel(tr("FundOS could not complete the import. Please try again."), page);
		auto* close  = new QPushButton(tr("Close"), page);

		auto* button_row = new QHBoxLayout();
		button_row->addStretch();
		button_row->addWidget(close);

		detail->setAlignment(Qt::AlignCenter);
		detail->setWordWrap(true);

		vbox->addStretch();
		vbox->addWidget(detail);
		vbox->addStretch();
		vbox->addLayout(button_row);

		connect(close, &QPushButton::clicked, this, &ImportDialog::reject);

		while (QLayoutItem* item = layout->takeAt(0)) {
			delete item->widget();
			delete item;
		}
		layout->addWidget(page);
		adjustSize();
		return;
	}

	int32_t total  = 0;
	int32_t merged = 0;
	for (const auto& bank_account : importing->accounts) {
		for (const auto& txn : bank_account.transactions) {
			++total;
			if (txn.get_match() != nullptr) { ++merged; }
		}
	}

	show_success_page(total - merged, merged);
}

fundos::import::bank_account* ImportDialog::AccountIterator::advance_to_unmatched(
	fundos::import::pending_import& pending,
	const std::vector<fundos::account>& accounts
) {
	for (; position < pending.accounts.size(); ++position) {
		bool matched = false;
		for (const auto& account : accounts) {
			if (pending.accounts[position].acct_id == account.bank_account_id) {
				matched = true;
				break;
			}
		}
		if (!matched) { return &pending.accounts[position]; }
	}
	return nullptr;
}
