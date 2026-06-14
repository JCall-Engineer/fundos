#pragma once
#include <unordered_map>
#include "fundos.hpp"
#include "database.hpp"
#include <QObject>

class AppContext : public QObject {
	Q_OBJECT

	friend class AppCoordinator;

	AppDatabase* app_database;

	std::optional<fundos::currency_locale::selection>   currency;
	std::optional<fundos::percentage_locale::selection> percentage;

	std::optional<std::vector<fundos::account>> account_list;
	std::optional<std::vector<fundos::fund>>    fund_list;
	std::optional<std::vector<fundos::budget>>  budget_list;

	std::unordered_map<int64_t, fundos::account*> account_by_id;
	std::unordered_map<int64_t, fundos::fund*>    fund_by_id;
	std::unordered_map<int64_t, fundos::budget*>  budget_by_id;

	AppContext() = delete;
	AppContext(const AppContext&) = delete;
	AppContext& operator=(const AppContext&) = delete;

	inline void populate_maps() {
		account_by_id.clear();
		fund_by_id.clear();
		budget_by_id.clear();
		if (!account_list || !fund_list || !budget_list) { return; }
		for (auto& record : *account_list) { account_by_id[record.id()] = &record; }
		for (auto& record : *fund_list)    { fund_by_id[record.id()]    = &record; }
		for (auto& record : *budget_list)  { budget_by_id[record.id()]  = &record; }
	}

	template<typename T>
	static T* find_item(const std::unordered_map<int64_t, T*>& lookup, int64_t id) {
		auto iterator = lookup.find(id);
		return iterator != lookup.end() ? iterator->second : nullptr;
	}

	/// Used to make the constructor private but accessible to make_shared.
	struct private_tag {};

public:
	explicit AppContext(private_tag, AppDatabase* database) : app_database(database) {}

	AppDatabase* database() const { return app_database; }

	const fundos::currency_locale::selection&   currency_locale()   const { return *currency; }
	const fundos::percentage_locale::selection& percentage_locale() const { return *percentage; }

	const std::vector<fundos::account>& accounts() const { return *account_list; }
	const std::vector<fundos::fund>&    funds()    const { return *fund_list; }
	const std::vector<fundos::budget>&  budgets()  const { return *budget_list; }

	const fundos::account* account(int64_t id) const { return find_item(account_by_id, id); }
	const fundos::fund*    fund   (int64_t id) const { return find_item(fund_by_id, id); }
	const fundos::budget*  budget (int64_t id) const { return find_item(budget_by_id, id); }

	// MainWindow needs access to the optional form of locales so it can show the locale editor

	const std::optional<fundos::currency_locale::selection>&   optional_currency_locale()   const { return currency; }
	const std::optional<fundos::percentage_locale::selection>& optional_percentage_locale() const { return percentage; }
};
