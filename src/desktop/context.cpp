#include "context.hpp"
#include <QApplication>
#include <QMessageBox>
#include <QString>

const std::shared_ptr<fundos::db>&          AppContext::db()                const { return database; }
const fundos::currency_locale::selection&   AppContext::currency_locale()   const { return *currency; }
const fundos::percentage_locale::selection& AppContext::percentage_locale() const { return *percentage; }

const std::vector<fundos::account>& AppContext::accounts() const { return account_list; }
const std::vector<fundos::fund>&    AppContext::funds()    const { return fund_list; }
const std::vector<fundos::budget>&  AppContext::budgets()  const { return budget_list; }

template<typename T>
static T* find_item(const std::unordered_map<int64_t, T*>& lookup, int64_t id) {
	auto iterator = lookup.find(id);
	return iterator != lookup.end() ? iterator->second : nullptr;
}

const fundos::account* AppContext::account(int64_t id) const { return find_item(account_by_id, id); }
const fundos::fund*    AppContext::fund   (int64_t id) const { return find_item(fund_by_id, id); }

enum class fetch_code : uint8_t {
	success,
	not_found,
	aborted,
	fatal,
};

/// Attempts to fetch a value from the database, retrying on transient errors.
/// Shows a retry/abort message box on each failure.
/// Calls on_fatal and returns fatal if the database becomes unready during retries.
/// Calls QApplication::quit() and returns aborted if the user chooses to abort.
/// Returns not_found without calling on_success if the query returns error::not_found.
/// Calls on_success with the fetched value and returns success on the happy path.
/// @param database The database to fetch from.
/// @param parent The widget to attach message boxes to.
/// @param fetch Member function pointer to the query method to call.
/// @param on_fatal Called with the outcome when the database becomes unready.
/// @param on_success Called with the fetched value on success.
/// @return A fetch_code indicating the outcome.
template<typename T>
static fetch_code try_fetch(
	std::shared_ptr<fundos::db>                     database,
	QWidget*                                        parent,
	fundos::db::result<T> (fundos::db::*fetch)(),
	std::function<void(const fundos::db::outcome&)> on_fatal,
	std::function<void(T&)>                         on_success
) {
	fundos::db::result<T> result = (database.get()->*fetch)();
	while (!result) {
		if (result.status().code == fundos::db::error::not_found) {
			return fetch_code::not_found;
		}
		if (!database->is_ready()) {
			on_fatal(result.status());
			return fetch_code::fatal;
		}
		QString detail = QObject::tr("Error Code:") + " " + QString::number(static_cast<int>(result.status().code));
		if (result.status().msg.has_value()) {
			const auto& view = result.status().msg->view();
			detail += "\n\n" + QObject::tr("Error Message:") + " " + QString::fromUtf8(view.data(), view.size());
		}
		int choice = QMessageBox::critical(
			parent,
			QObject::tr("Settings Error"),
			QObject::tr("FundOS could not read its settings from the database and cannot continue.") + "\n\n" + detail,
			QMessageBox::StandardButton::Retry,
			QMessageBox::StandardButton::Abort
		);
		if (choice == QMessageBox::Abort) {
			QApplication::quit();
			return fetch_code::aborted;
		}
		result = (database.get()->*fetch)();
	}
	on_success(result.value());
	return fetch_code::success;
}

void AppContext::try_create(std::shared_ptr<fundos::db> database, QWidget* parent_widget, const creation_handles& handles) {
	if (!parent_widget)        { FUNDOS_ASSERT(false, "try_create called with null parent");  return; }
	if (!database)             { FUNDOS_ASSERT(false, "try_create called with null db");      return; }
	if (!database->is_ready()) { FUNDOS_ASSERT(false, "try_create called with db not ready"); return; }

	std::shared_ptr<AppContext> out = std::make_shared<AppContext>(private_tag{}, handles.on_fatal, parent_widget);
	out->database = database;

	fetch_code fetch_currency = try_fetch<fundos::currency_locale::selection>(database, parent_widget, &fundos::db::get_currency_locale, handles.on_fatal, [&out](auto& locale) {
		out->currency = locale;
	});
	switch (fetch_currency) {
		case fetch_code::success:
		case fetch_code::not_found: // continue to try and get percentage locale, we will later guard against not found
			break;
		case fetch_code::aborted:
		case fetch_code::fatal:
			return;
	}

	fetch_code fetch_percentage = try_fetch<fundos::percentage_locale::selection>(database, parent_widget, &fundos::db::get_percentage_locale, handles.on_fatal, [&out](auto& locale) {
		out->percentage = locale;
	});
	switch (fetch_percentage) {
		case fetch_code::success:
		case fetch_code::not_found: // the guard is after this switch
			break;
		case fetch_code::aborted:
		case fetch_code::fatal:
			return;
	}

	if (!out->currency || !out->percentage) {
		return handles.on_needs_locale(out);
	}

	fetch_code accounts_fetch = try_fetch<std::vector<fundos::account>>(database, parent_widget, &fundos::db::get_accounts, handles.on_fatal, [&out](auto& list) {
		out->account_list = std::move(list);
	});
	switch (accounts_fetch) {
		case fetch_code::not_found:
			FUNDOS_UNREACHABLE();
		case fetch_code::success:
			break;
		case fetch_code::aborted:
		case fetch_code::fatal:
			return;
	}

	fetch_code funds_fetch = try_fetch<std::vector<fundos::fund>>(database, parent_widget, &fundos::db::get_funds, handles.on_fatal, [&out](auto& list) {
		out->fund_list = std::move(list);
	});
	switch (funds_fetch) {
		case fetch_code::not_found:
			FUNDOS_UNREACHABLE();
		case fetch_code::success:
			break;
		case fetch_code::aborted:
		case fetch_code::fatal:
			return;
	}

	fetch_code budgets_fetch = try_fetch<std::vector<fundos::budget>>(database, parent_widget, &fundos::db::get_budgets, handles.on_fatal, [&out](auto& list) {
		out->budget_list = std::move(list);
	});
	switch (budgets_fetch) {
		case fetch_code::not_found:
			FUNDOS_UNREACHABLE();
		case fetch_code::success:
			break;
		case fetch_code::aborted:
		case fetch_code::fatal:
			return;
	}

	out->initialize();
	return handles.on_success(out);
}

void AppContext::initialize() {
	for (auto& record : account_list) {
		account_by_id[record.id()] = &record;
	}
	for (auto& record : fund_list) {
		fund_by_id[record.id()] = &record;
	}
	for (auto& record : budget_list) {
		budget_by_id[record.id()] = &record;
	}
}

void AppContext::refresh_locale() {
	std::shared_ptr<AppContext> out = std::make_shared<AppContext>(private_tag{}, on_fatal, parent_widget);
	out->database = database;

	fetch_code fetch_currency = try_fetch<fundos::currency_locale::selection>(database, parent_widget, &fundos::db::get_currency_locale, on_fatal, [&out](auto& locale) {
		out->currency = locale;
	});
	switch (fetch_currency) {
		case fetch_code::success:
			break;
		case fetch_code::not_found:
			FUNDOS_UNREACHABLE();
		case fetch_code::aborted:
		case fetch_code::fatal:
			return;
	}

	fetch_code fetch_percentage = try_fetch<fundos::percentage_locale::selection>(database, parent_widget, &fundos::db::get_percentage_locale, on_fatal, [&out](auto& locale) {
		out->percentage = locale;
	});
	switch (fetch_percentage) {
		case fetch_code::success:
			break;
		case fetch_code::not_found:
			FUNDOS_UNREACHABLE();
		case fetch_code::aborted:
		case fetch_code::fatal:
			return;
	}

	out->account_list = account_list;
	out->fund_list = fund_list;
	out->budget_list = budget_list;
	out->initialize();
	emit refreshed(out);
}

void AppContext::refresh_accounts() {
	std::shared_ptr<AppContext> out = std::make_shared<AppContext>(private_tag{}, on_fatal, parent_widget);
	out->database = database;
	out->currency = currency;
	out->percentage = percentage;

	fetch_code accounts_fetch = try_fetch<std::vector<fundos::account>>(database, parent_widget, &fundos::db::get_accounts, on_fatal, [&out](auto& list) {
		out->account_list = std::move(list);
	});
	switch (accounts_fetch) {
		case fetch_code::success:
			break;
		case fetch_code::not_found:
			FUNDOS_UNREACHABLE();
		case fetch_code::aborted:
		case fetch_code::fatal:
			return;
	}

	out->fund_list = fund_list;
	out->budget_list = budget_list;
	out->initialize();
	emit refreshed(out);
}
void AppContext::refresh_funds() {
	std::shared_ptr<AppContext> out = std::make_shared<AppContext>(private_tag{}, on_fatal, parent_widget);
	out->database = database;
	out->currency = currency;
	out->percentage = percentage;
	out->account_list = account_list;

	fetch_code funds_fetch = try_fetch<std::vector<fundos::fund>>(database, parent_widget, &fundos::db::get_funds, on_fatal, [&out](auto& list) {
		out->fund_list = std::move(list);
	});
	switch (funds_fetch) {
		case fetch_code::success:
			break;
		case fetch_code::not_found:
			FUNDOS_UNREACHABLE();
		case fetch_code::aborted:
		case fetch_code::fatal:
			return;
	}

	out->budget_list = budget_list;
	out->initialize();
	emit refreshed(out);
}
void AppContext::refresh_budgets() {
	std::shared_ptr<AppContext> out = std::make_shared<AppContext>(private_tag{}, on_fatal, parent_widget);
	out->database = database;
	out->currency = currency;
	out->percentage = percentage;
	out->account_list = account_list;
	out->fund_list = fund_list;

	fetch_code budgets_fetch = try_fetch<std::vector<fundos::budget>>(database, parent_widget, &fundos::db::get_budgets, on_fatal, [&out](auto& list) {
		out->budget_list = std::move(list);
	});
	switch (budgets_fetch) {
		case fetch_code::success:
			break;
		case fetch_code::not_found:
			FUNDOS_UNREACHABLE();
		case fetch_code::aborted:
		case fetch_code::fatal:
			return;
	}

	out->initialize();
	emit refreshed(out);
}

void AppContext::update_account(const fundos::account& updating) {
	if (!find_item(account_by_id, updating.id())) { return; }

	std::shared_ptr<AppContext> out = std::make_shared<AppContext>(private_tag{}, on_fatal, parent_widget);
	out->database = database;
	out->currency = currency;
	out->percentage = percentage;
	out->account_list = account_list;
	out->fund_list = fund_list;
	out->budget_list = budget_list;
	out->initialize();

	*find_item(out->account_by_id, updating.id()) = updating;

	emit refreshed(out);

}
void AppContext::update_fund(const fundos::fund& updating) {
	if (!find_item(fund_by_id, updating.id())) { return; }

	std::shared_ptr<AppContext> out = std::make_shared<AppContext>(private_tag{}, on_fatal, parent_widget);
	out->database = database;
	out->currency = currency;
	out->percentage = percentage;
	out->account_list = account_list;
	out->fund_list = fund_list;
	out->budget_list = budget_list;
	out->initialize();

	*find_item(out->fund_by_id, updating.id()) = updating;

	emit refreshed(out);

}
void AppContext::update_budget(const fundos::budget& updating) {
	if (!find_item(budget_by_id, updating.id())) { return; }

	std::shared_ptr<AppContext> out = std::make_shared<AppContext>(private_tag{}, on_fatal, parent_widget);
	out->database = database;
	out->currency = currency;
	out->percentage = percentage;
	out->account_list = account_list;
	out->fund_list = fund_list;
	out->budget_list = budget_list;
	out->initialize();

	*find_item(out->budget_by_id, updating.id()) = updating;

	emit refreshed(out);
}
