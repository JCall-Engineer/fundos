#include "home_page.hpp"
#include "theme.hpp"
#include "components/navigable_row.hpp"
#include "content/budget_dialog.hpp"
#include "content/import_dialog.hpp"
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QToolButton>
#include <QScrollArea>

HomePage::HomePage(AppCoordinator* coordinator, QWidget* parent) : QWidget(parent), app_coordinator(std::move(coordinator)) {
	auto* root_layout = new QVBoxLayout(this);
	root_layout->setContentsMargins(0, 0, 0, 0);

	connect(this, &HomePage::toggle_closed_accounts, this, [this](bool visible) {
		show_closed_accounts = visible;
	});
	connect(this, &HomePage::toggle_closed_funds,    this, [this](bool visible) {
		show_closed_funds = visible;
	});

	scroll_area = new QScrollArea(this);
	scroll_area->setWidgetResizable(true);
	scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	root_layout->addWidget(scroll_area);

	auto* database = app_coordinator->database();
	connect(database, &AppDatabase::accounts_received,      this, &HomePage::on_accounts);
	connect(database, &AppDatabase::funds_received,         this, &HomePage::on_funds);
	connect(database, &AppDatabase::budgets_received,       this, &HomePage::on_budgets);
	connect(database, &AppDatabase::account_saved,          this, &HomePage::on_account_created);
	connect(database, &AppDatabase::fund_saved,             this, &HomePage::on_fund_created);
	connect(this, &HomePage::create_account,            database, &AppDatabase::save_account);
	connect(this, &HomePage::create_fund,               database, &AppDatabase::save_fund);
	connect(this, &HomePage::account_balance_requested, database, &AppDatabase::request_account_balance);
	connect(this, &HomePage::fund_balance_requested,    database, &AppDatabase::request_fund_balance);
	connect(this, &HomePage::accounts_requested,        database, &AppDatabase::request_accounts);
	connect(this, &HomePage::funds_requested,           database, &AppDatabase::request_funds);
	connect(this, &HomePage::budgets_requested,         database, &AppDatabase::request_budgets);

	account_list = new QWidget(this);
	{
		auto* layout = new QVBoxLayout(account_list);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		layout->setAlignment(Qt::AlignTop);
	}
	account_panel = make_panel(account_list, tr("ACCOUNTS"), {
		{
			.tooltip   = QString("Import OFX File"),
			.icon_path = QString(":/icons/upload.svg"),
			.action    = [this]() {
				auto* dialog = new ImportDialog(app_coordinator, this);
				dialog->setAttribute(Qt::WA_DeleteOnClose);
				connect(dialog, &QDialog::accepted, this, [this]() {
					make_accounts(app_coordinator->context()->accounts());
				});
				dialog->show();
			},
		},
		{
			.tooltip           = QString("Toggle Closed Accounts"),
			.icon_path         = QString(":/icons/eye-off.svg"),
			.checked_icon_path = QString(":/icons/eye.svg"),
			.toggle_signal     = &HomePage::toggle_closed_accounts,
		},
		{
			.tooltip   = QString("Create Account"),
			.icon_path = QString(":/icons/plus.svg"),
			.action    = [this]() {
				bool accepted = false;
				QString name = QInputDialog::getText(this, tr("New Account"), tr("Account name:"), QLineEdit::Normal, "", &accepted);
				if (!accepted) { return; }
				name = name.trimmed();
				if (name.isEmpty()) { return; }

				fundos::account creating = { .name = name.toStdString() };
				emit create_account(creating);
			},
		},
	});

	fund_list = new QWidget(this);
	{
		auto* layout = new QVBoxLayout(fund_list);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		layout->setAlignment(Qt::AlignTop);
	}
	fund_panel = make_panel(fund_list, tr("FUNDS"), {
		{
			.tooltip           = QString("Toggle Closed Funds"),
			.icon_path         = QString(":/icons/eye-off.svg"),
			.checked_icon_path = QString(":/icons/eye.svg"),
			.toggle_signal     = &HomePage::toggle_closed_funds,
		},
		{
			.tooltip   = QString("Create Fund"),
			.icon_path = QString(":/icons/plus.svg"),
			.action    = [this]() {
				bool accepted = false;
				QString name = QInputDialog::getText(this, tr("New Fund"), tr("Fund name:"), QLineEdit::Normal, "", &accepted);
				if (!accepted) { return; }
				name = name.trimmed();
				if (name.isEmpty()) { return; }

				fundos::fund creating = { .name = name.toStdString() };
				emit create_fund(creating);
			},
		},
	});

	budget_list = new QWidget(this);
	{
		auto* layout = new QVBoxLayout(budget_list);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		layout->setAlignment(Qt::AlignTop);
	}
	budget_panel = make_panel(budget_list, tr("BUDGETS"), {
		{
			.tooltip   = QString("Create Fund"),
			.icon_path = QString(":/icons/plus.svg"),
			.action    = [this]() {
				bool accepted = false;
				QString name = QInputDialog::getText(this, tr("New Fund"), tr("Fund name:"), QLineEdit::Normal, "", &accepted);
				if (!accepted) { return; }
				name = name.trimmed();
				if (name.isEmpty()) { return; }

				auto* dialog = new BudgetDialog(app_coordinator, fundos::budget{ .name = name.toStdString() }, this);
				dialog->setAttribute(Qt::WA_DeleteOnClose);
				connect(dialog, &QDialog::accepted, this, [this]() {
					emit budgets_requested();
				});
				dialog->show();
			},
		},
	});

	make_accounts(app_coordinator->context()->accounts());
	make_funds(app_coordinator->context()->funds());
	make_budgets(app_coordinator->context()->budgets());
	relayout();
}

QWidget* HomePage::make_panel(QWidget* list, const QString& title, std::vector<button_spec> buttons) {
	auto* panel = new QWidget(this);
	auto* layout = new QVBoxLayout(panel);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	auto* header = new QHBoxLayout();
	auto* label = theme::header_label(title, panel);

	header->addWidget(label);
	header->addStretch();

	for (auto& spec : buttons) {
		auto* button = new QToolButton(panel);
		button->setToolTip(spec.tooltip);
		button->setIcon(theme::colored_svg_icon(spec.icon_path, theme::text, theme::toolbar_icon_size));
		button->setIconSize(theme::label_icon_size(label));
		button->setAutoRaise(true);
		if (spec.toggle_signal && !spec.checked_icon_path.isEmpty()) {
			button->setStyleSheet("QToolButton:checked { background: transparent; border: none; }");
			button->setCheckable(true);
			button->setChecked(false);
			connect(button, &QToolButton::toggled, this, spec.toggle_signal);
			connect(button, &QToolButton::toggled, this, [button, spec](bool checked) {
				const auto& path = checked ? spec.checked_icon_path : spec.icon_path;
				button->setIcon(theme::colored_svg_icon(path, theme::text, theme::toolbar_icon_size));
			});
		}
		if (spec.action) {
			connect(button, &QToolButton::clicked, this, spec.action);
		}
		header->addWidget(button);
	}

	list->setParent(panel);
	layout->addLayout(header);
	layout->addWidget(list);

	return panel;
}

void HomePage::make_accounts(const std::vector<fundos::account>& accounts) {
	auto* database = app_coordinator->database();
	auto* layout = account_list->layout();
	while (QLayoutItem* item = layout->takeAt(0)) {
		delete item->widget();
		delete item;
	}

	for (size_t i = 0; i < accounts.size(); ++i) {
		const auto& record = accounts[i];
		const int64_t id = record.id();

		auto props = NavigableRow::props{
			.index = i,
			.is_closed = record.closed_at.has_value(),
		};

		auto* row = new NavigableRow(props, QString::fromStdString(accounts[i].name), this);
		connect(row, &NavigableRow::clicked, this, [this](size_t index) {
			emit open_account(app_coordinator->context()->accounts()[index]);
		});
		connect(this, &HomePage::toggle_closed_accounts, row, &NavigableRow::on_toggle);
		row->on_toggle(show_closed_accounts);

		connect(database, &AppDatabase::account_balance_received, row, [this, row, id](int64_t account_id, fundos::db::result<fundos::currency> amount) {
			if (amount && account_id == id) {
				row->set_amount(theme::currency_label(amount.value(), app_coordinator->context()->currency_locale().info()), amount.value().minor_units != 0);
			}
		});
		emit account_balance_requested(id);

		layout->addWidget(row);
	}
}
void HomePage::make_funds(const std::vector<fundos::fund>& funds) {
	auto* database = app_coordinator->database();
	auto* layout = fund_list->layout();
	while (QLayoutItem* item = layout->takeAt(0)) {
		delete item->widget();
		delete item;
	}

	for (size_t i = 0; i < funds.size(); ++i) {
		const auto& record = funds[i];
		const int64_t id = record.id();

		auto props = NavigableRow::props{
			.index = i,
			.is_closed = record.closed_at.has_value(),
		};

		auto* row = new NavigableRow(props, QString::fromStdString(record.name), this);
		connect(row, &NavigableRow::clicked, this, [this](size_t index) {
			emit open_fund(app_coordinator->context()->funds()[index]);
		});
		connect(this, &HomePage::toggle_closed_funds, row, &NavigableRow::on_toggle);
		row->on_toggle(show_closed_funds);

		connect(database, &AppDatabase::fund_balance_received, row, [this, row, id](int64_t fund_id, fundos::db::result<fundos::currency> amount) {
			if (amount && fund_id == id) {
				row->set_amount(theme::currency_label(amount.value(), app_coordinator->context()->currency_locale().info()), amount.value().minor_units != 0);
			}
		});
		emit fund_balance_requested(id);

		layout->addWidget(row);
	}
}
void HomePage::make_budgets(const std::vector<fundos::budget>& budgets) {
	auto* layout = budget_list->layout();
	while (QLayoutItem* item = layout->takeAt(0)) {
		delete item->widget();
		delete item;
	}

	for (size_t i = 0; i < budgets.size(); ++i) {
		const auto& record = budgets[i];

		auto props = NavigableRow::props{
			.index = i,
			.is_closed = false,
		};

		auto* row = new NavigableRow(props, QString::fromStdString(record.name), this);
		connect(row, &NavigableRow::clicked, this, [this](size_t index) {
			auto* dialog = new BudgetDialog(app_coordinator, app_coordinator->context()->budgets()[index], this);
			dialog->setAttribute(Qt::WA_DeleteOnClose);
			connect(dialog, &QDialog::accepted, this, [this]() {
				emit budgets_requested();
			});
			dialog->show();
		});
		layout->addWidget(row);
	}
}

void HomePage::on_account_created(fundos::db::outcome saved) { if (saved) { emit accounts_requested(); } }
void HomePage::on_fund_created   (fundos::db::outcome saved) { if (saved) { emit funds_requested();    } }

void HomePage::on_accounts(fundos::db::result<std::vector<fundos::account>> accounts) { if (accounts) { make_accounts(accounts.value()); } }
void HomePage::on_funds   (fundos::db::result<std::vector<fundos::fund>>    funds)    { if (funds)    { make_funds   (funds.value());    } }
void HomePage::on_budgets (fundos::db::result<std::vector<fundos::budget>> budgets)   { if (budgets)  { make_budgets (budgets.value());  } }

void HomePage::resizeEvent(QResizeEvent* event) {
	QWidget::resizeEvent(event);
	relayout();
}

static constexpr int HORIZONTAL_BREAKPOINT = 900;
void HomePage::relayout() {
	if (budget_panel == nullptr) { return; } // budget_list is the last pointer set in the ctor

	bool use_horizontal = width() >= HORIZONTAL_BREAKPOINT;

	auto direction = use_horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom;
	auto divider_shape = use_horizontal ? QFrame::VLine : QFrame::HLine;

	auto* container = new QWidget();
	auto* container_layout = new QBoxLayout(direction, container);
	container_layout->setContentsMargins(8, 8, 8, 8);
	container_layout->setSpacing(8);

	container_layout->addWidget(account_panel);

	auto* divider1 = new QFrame(container);
	divider1->setFrameShape(divider_shape);
	divider1->setFrameShadow(QFrame::Plain);
	container_layout->addWidget(divider1);

	container_layout->addWidget(fund_panel);
	
	auto* divider2 = new QFrame(container);
	divider2->setFrameShape(divider_shape);
	divider2->setFrameShadow(QFrame::Plain);
	container_layout->addWidget(divider2);

	container_layout->addWidget(budget_panel);

	// setWidget replaces the old container and deletes it,
	// but our list widgets are parented to HomePage so they survive
	scroll_area->setWidget(container);
}
