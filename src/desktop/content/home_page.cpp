#include "home_page.hpp"
#include "theme.hpp"
#include "components/navigable_row.hpp"
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QToolButton>
#include <QScrollArea>

HomePage::HomePage(std::shared_ptr<AppContext> ctx, QWidget* parent) : QWidget(parent), context(std::move(ctx)) {
	auto* root_layout = new QVBoxLayout(this);
	root_layout->setContentsMargins(0, 0, 0, 0);

	scroll_area = new QScrollArea(this);
	scroll_area->setWidgetResizable(true);
	scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	root_layout->addWidget(scroll_area);
}

QWidget* HomePage::make_panel(QWidget* list, const QString& title, std::vector<button_spec> buttons) {
	auto* panel = new QWidget(this);
	auto* layout = new QVBoxLayout(panel);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	auto* header = new QHBoxLayout();
	auto* label = theme::header_label(title, panel);
	const QSize button_size(label->sizeHint().height(), label->sizeHint().height());

	header->addWidget(label);
	header->addStretch();

	for (auto& spec : buttons) {
		auto* button = new QToolButton(panel);
		button->setToolTip(spec.tooltip);
		button->setIcon(theme::colored_svg_icon(spec.icon_path, theme::text, button_size));
		button->setIconSize(button_size);
		button->setAutoRaise(true);
		if (spec.toggle_signal && !spec.checked_icon_path.isEmpty()) {
			button->setStyleSheet("QToolButton:checked { background: transparent; border: none; }");
			button->setCheckable(true);
			button->setChecked(false);
			connect(button, &QToolButton::toggled, this, spec.toggle_signal);
			connect(button, &QToolButton::toggled, this, [button, spec, button_size](bool checked) {
				const auto& path = checked ? spec.checked_icon_path : spec.icon_path;
				button->setIcon(theme::colored_svg_icon(path, theme::text, button_size));
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

void HomePage::initialize() {
	auto* accounts_list = new QWidget(this);
	{
		auto* layout = new QVBoxLayout(accounts_list);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		layout->setAlignment(Qt::AlignTop);

		const auto& accounts = context->accounts();
		for (size_t i = 0; i < accounts.size(); ++i) {
			const auto& record = accounts[i];
			auto props = NavigableRow::props{
				.index = i,
				.is_closed = record.closed_at.has_value(),
			};
			auto balance = context->db()->get_account_balance(record.id());
			QLabel* amount = nullptr;
			if (balance) {
				amount = theme::currency_label(balance.value(), context->currency_locale().info());
				if (balance.value().minor_units != 0) {
					props.has_amount = true;
				}
			} else {
				emit db_outcome(balance.status());
			}

			auto* row = new NavigableRow(props, QString::fromStdString(accounts[i].name), amount, this);
			connect(row, &NavigableRow::clicked, this, [this](size_t index) {
				emit open_account(context->accounts()[index]);
			});
			connect(this, &HomePage::toggle_closed_accounts, row, &NavigableRow::on_toggle);
			row->on_toggle(false); // set the initial state
			layout->addWidget(row);
		}
	}
	account_panel = make_panel(accounts_list, tr("ACCOUNTS"), {
		{
			.tooltip   = QString("Import OFX File"),
			.icon_path = QString(":/icons/upload.svg"),
			.action    = [this]() { emit import_ofx(); },
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
				auto saved = context->db()->save_account(creating);
				if (!saved) {
					emit db_outcome(saved);
					return;
				}
				context->refresh_accounts();
				emit refresh();
			},
		},
	});

	auto* funds_list = new QWidget(this);
	{
		auto* layout = new QVBoxLayout(funds_list);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		layout->setAlignment(Qt::AlignTop);

		const auto& funds = context->funds();
		for (size_t i = 0; i < funds.size(); ++i) {
			const auto& record = funds[i];

			auto props = NavigableRow::props{
				.index = i,
				.is_closed = record.closed_at.has_value(),
			};

			auto balance = context->db()->get_fund_balance(record.id());
			QLabel* amount = nullptr;
			if (balance) {
				amount = theme::currency_label(balance.value(), context->currency_locale().info());
				if (balance.value().minor_units != 0) {
					props.has_amount = true;
				}
			} else {
				emit db_outcome(balance.status());
			}

			auto* row = new NavigableRow(props, QString::fromStdString(record.name), amount, this);
			connect(row, &NavigableRow::clicked, this, [this](size_t index) {
				emit open_fund(context->funds()[index]);
			});
			connect(this, &HomePage::toggle_closed_funds, row, &NavigableRow::on_toggle);
			row->on_toggle(false); // set the initial state
			layout->addWidget(row);
		}
	}
	fund_panel = make_panel(funds_list, tr("FUNDS"), {
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
				auto saved = context->db()->save_fund(creating);
				if (!saved) {
					emit db_outcome(saved);
					return;
				}
				context->refresh_funds();
				emit refresh();
			},
		},
	});

	auto* budget_list = new QWidget(this);
	{
		auto* layout = new QVBoxLayout(budget_list);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		layout->setAlignment(Qt::AlignTop);

		const auto& budgets = context->budgets();
		for (size_t i = 0; i < budgets.size(); ++i) {
			const auto& record = budgets[i];

			auto props = NavigableRow::props{
				.index = i,
				.is_closed = false,
			};

			auto* row = new NavigableRow(props, QString::fromStdString(record.name), nullptr, this);
			connect(row, &NavigableRow::clicked, this, [this](size_t index) {
				emit open_budget(context->budgets()[index]);
			});
			layout->addWidget(row);
		}
	}
	budget_panel = make_panel(budget_list, tr("BUDGETS"), {
		{
			.tooltip   = QString("Create Fund"),
			.icon_path = QString(":/icons/plus.svg"),
			.action    = [this]() {
				emit open_budget(fundos::budget{});
			},
		},
	});

	relayout();
}

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
