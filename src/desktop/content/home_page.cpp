#include "home_page.hpp"
#include "theme.hpp"
#include "components/account_list.hpp"
#include "components/fund_list.hpp"
#include "components/budget_list.hpp"
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
		button->setIcon(theme::colored_svg_icon(spec.icon_path, theme::text, button_size));
		button->setAutoRaise(true);
		connect(button, &QToolButton::clicked, this, spec.action);
		header->addWidget(button);
	}

	list->setParent(panel);
	layout->addLayout(header);
	layout->addWidget(list);

	return panel;
}

void HomePage::initialize() {
	auto* accounts = new AccountList(context, this);
	connect(accounts, &AccountList::db_outcome,   this, &HomePage::db_outcome);
	connect(accounts, &AccountList::open_account, this, &HomePage::open_account);
	accounts->initialize();
	account_panel = make_panel(accounts, tr("ACCOUNTS"), {
		{
			QString(":/icons/upload.svg"),
			[this]() { emit import_ofx(); },
		},
		{
			QString(":/icons/plus.svg"),
			[this, accounts]() {
				bool accepted = false;
				QString name = QInputDialog::getText(this, tr("New Account"), tr("Account name:"), QLineEdit::Normal, "", &accepted);
				if (!accepted) { return; }
				name = name.trimmed();
				if (name.isEmpty()) { return; }

				fundos::account creating = { .name = name.toStdString() };
				auto saved = context->database->save_account(creating);
				if (!saved) {
					emit db_outcome(saved);
					return;
				}
				accounts->initialize();
			},
		},
	});

	auto* funds = new FundList(context, this);
	connect(funds, &FundList::db_outcome, this, &HomePage::db_outcome);
	connect(funds, &FundList::open_fund, this,  &HomePage::open_fund);
	funds->initialize();
	fund_panel = make_panel(funds, tr("FUNDS"), {
		{
			QString(":/icons/plus.svg"),
			[this, funds]() {
				bool accepted = false;
				QString name = QInputDialog::getText(this, tr("New Fund"), tr("Fund name:"), QLineEdit::Normal, "", &accepted);
				if (!accepted) { return; }
				name = name.trimmed();
				if (name.isEmpty()) { return; }

				fundos::fund creating = { .name = name.toStdString() };
				auto saved = context->database->save_fund(creating);
				if (!saved) {
					emit db_outcome(saved);
					return;
				}
				funds->initialize();
			},
		},
	});

	auto* budgets = new BudgetList(context, this);
	connect(budgets, &BudgetList::db_outcome,  this, &HomePage::db_outcome);
	connect(budgets, &BudgetList::open_budget, this, &HomePage::open_budget);
	budget_panel = make_panel(budgets, tr("BUDGETS"), {
		{
			QString(":/icons/plus.svg"),
			[this]() {
				auto creating = std::make_shared<fundos::budget>();
				emit open_budget(creating);
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
