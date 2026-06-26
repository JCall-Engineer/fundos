#include "coordinator.hpp"
#include <QApplication>
#include <QMessageBox>
#include <QString>

AppCoordinator::AppCoordinator(
	AppDatabase* database,
	QWidget* parent
) : QObject(parent), parent_widget(parent) , app_database(std::move(database)) {
	current_context = std::make_shared<AppContext>(AppContext::private_tag{}, app_database);
	connect(app_database, &AppDatabase::locales_received,  this, &AppCoordinator::on_locales_received);
	connect(app_database, &AppDatabase::accounts_received, this, &AppCoordinator::on_accounts_received);
	connect(app_database, &AppDatabase::funds_received,    this, &AppCoordinator::on_funds_received);
	connect(app_database, &AppDatabase::budgets_received,  this, &AppCoordinator::on_budgets_received);

	connect(this, &AppCoordinator::locales_requested,  app_database, &AppDatabase::request_locales);
	connect(this, &AppCoordinator::accounts_requested, app_database, &AppDatabase::request_accounts);
	connect(this, &AppCoordinator::funds_requested,    app_database, &AppDatabase::request_funds);
	connect(this, &AppCoordinator::budgets_requested,  app_database, &AppDatabase::request_budgets);
}

void AppCoordinator::on_database_open() {
	current_context = std::make_shared<AppContext>(AppContext::private_tag{}, app_database);

	emit locales_requested();
	emit accounts_requested();
	emit funds_requested();
	emit budgets_requested();
}

bool AppCoordinator::show_retry_dialog(const fundos::db::outcome& outcome) {
	QString detail = QObject::tr("Error Code:") + " " + QString::number(static_cast<int>(outcome.code));
	if (outcome.msg.has_value()) {
		const auto& view = outcome.msg->view();
		detail += "\n\n" + QObject::tr("Error Message: \"%1\"").arg(QString::fromUtf8(view.data(), view.size()));
	}
	int choice = QMessageBox::critical(
		parent_widget,
		QObject::tr("Database Error"),
		QObject::tr("FundOS could not read from the database and cannot continue.") + "\n\n" + detail,
		QMessageBox::StandardButton::Retry,
		QMessageBox::StandardButton::Abort
	);
	return choice == QMessageBox::StandardButton::Retry;
}

void AppCoordinator::on_locales_received(
	fundos::db::result<fundos::currency_locale::selection>   currency,
	fundos::db::result<fundos::percentage_locale::selection> percentage
) {
	if (!currency && currency.status().code != fundos::db::error::not_found) {
		if (show_retry_dialog(currency.status())) {
			emit locales_requested();
		} else {
			emit creation_failure(currency.status());
		}
		return;
	}
	if (!percentage && percentage.status().code != fundos::db::error::not_found) {
		if (show_retry_dialog(percentage.status())) {
			emit locales_requested();
		} else {
			emit creation_failure(percentage.status());
		}
		return;
	}

	const bool was_ready = is_ready();

	auto next = std::make_shared<AppContext>(AppContext::private_tag{}, app_database);
	next->currency     = current_context->currency;
	next->percentage   = current_context->percentage;
	next->account_list = current_context->account_list;
	next->fund_list    = current_context->fund_list;
	next->budget_list  = current_context->budget_list;
	if (currency)   { next->currency   = currency.value(); }
	if (percentage) { next->percentage = percentage.value(); }
	next->populate_maps();
	current_context = next;

	if (!next->currency || !next->percentage) {
		emit needs_locale();
		return;
	}

	if (!is_ready()) { return; }
	if (!was_ready) {
		emit created();
	} else {
		emit refreshed();
	}
}

void AppCoordinator::on_accounts_received(fundos::db::result<std::vector<fundos::account>> accounts) {
	if (!accounts) {
		if (show_retry_dialog(accounts.status())) {
			emit accounts_requested();
		} else {
			emit creation_failure(accounts.status());
		}
		return;
	}

	const bool was_ready = is_ready();

	auto next = std::make_shared<AppContext>(AppContext::private_tag{}, app_database);
	next->currency     = current_context->currency;
	next->percentage   = current_context->percentage;
	next->account_list = std::move(accounts.value());
	next->fund_list    = current_context->fund_list;
	next->budget_list  = current_context->budget_list;
	next->populate_maps();
	current_context = next;

	if (!is_ready()) { return; }
	if (!was_ready) {
		emit created();
	} else {
		emit refreshed();
	}
}

void AppCoordinator::on_funds_received(fundos::db::result<std::vector<fundos::fund>> funds) {
	if (!funds) {
		if (show_retry_dialog(funds.status())) {
			emit funds_requested();
		} else {
			emit creation_failure(funds.status());
		}
		return;
	}

	const bool was_ready = is_ready();

	auto next = std::make_shared<AppContext>(AppContext::private_tag{}, app_database);
	next->currency     = current_context->currency;
	next->percentage   = current_context->percentage;
	next->account_list = current_context->account_list;
	next->fund_list    = std::move(funds.value());
	next->budget_list  = current_context->budget_list;
	next->populate_maps();
	current_context = next;

	if (!is_ready()) { return; }
	if (!was_ready) {
		emit created();
	} else {
		emit refreshed();
	}
}

void AppCoordinator::on_budgets_received(fundos::db::result<std::vector<fundos::budget>> budgets) {
	if (!budgets) {
		if (show_retry_dialog(budgets.status())) {
			emit budgets_requested();
		} else {
			emit creation_failure(budgets.status());
		}
		return;
	}

	const bool was_ready = is_ready();

	auto next = std::make_shared<AppContext>(AppContext::private_tag{}, app_database);
	next->currency     = current_context->currency;
	next->percentage   = current_context->percentage;
	next->account_list = current_context->account_list;
	next->fund_list    = current_context->fund_list;
	next->budget_list  = std::move(budgets.value());
	next->populate_maps();
	current_context = next;

	if (!is_ready()) { return; }
	if (!was_ready) {
		emit created();
	} else {
		emit refreshed();
	}
}

void AppCoordinator::update_locales(const fundos::currency_locale::selection& currency, const fundos::percentage_locale::selection& percentage) {
	const bool was_ready = is_ready();
	auto next = std::make_shared<AppContext>(AppContext::private_tag{}, app_database);
	next->currency     = currency;
	next->percentage   = percentage;
	next->account_list = current_context->account_list;
	next->fund_list    = current_context->fund_list;
	next->budget_list  = current_context->budget_list;
	next->populate_maps();
	current_context = next;

	if (!is_ready()) { return; }
	if (!was_ready) {
		emit created();
	} else {
		emit refreshed();
	}
}

void AppCoordinator::update_account(const fundos::account& updating) {
	if (!current_context->AppContext::find_item(current_context->account_by_id, updating.id())) { return; }
	auto next = std::make_shared<AppContext>(AppContext::private_tag{}, app_database);
	next->currency     = current_context->currency;
	next->percentage   = current_context->percentage;
	next->account_list = current_context->account_list;
	next->fund_list    = current_context->fund_list;
	next->budget_list  = current_context->budget_list;
	next->populate_maps();
	*AppContext::find_item(next->account_by_id, updating.id()) = updating;
	current_context = next;
	emit refreshed();
}

void AppCoordinator::update_fund(const fundos::fund& updating) {
	if (!current_context->AppContext::find_item(current_context->fund_by_id, updating.id())) { return; }
	auto next = std::make_shared<AppContext>(AppContext::private_tag{}, app_database);
	next->currency     = current_context->currency;
	next->percentage   = current_context->percentage;
	next->account_list = current_context->account_list;
	next->fund_list    = current_context->fund_list;
	next->budget_list  = current_context->budget_list;
	next->populate_maps();
	*AppContext::find_item(next->fund_by_id, updating.id()) = updating;
	current_context = next;
	emit refreshed();
}

void AppCoordinator::update_budget(const fundos::budget& updating) {
	if (!current_context->AppContext::find_item(current_context->budget_by_id, updating.id())) { return; }
	auto next = std::make_shared<AppContext>(AppContext::private_tag{}, app_database);
	next->currency     = current_context->currency;
	next->percentage   = current_context->percentage;
	next->account_list = current_context->account_list;
	next->fund_list    = current_context->fund_list;
	next->budget_list  = current_context->budget_list;
	next->populate_maps();
	*AppContext::find_item(next->budget_by_id, updating.id()) = updating;
	current_context = next;
	emit refreshed();
}
