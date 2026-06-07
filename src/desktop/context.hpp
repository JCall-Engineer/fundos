#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include "fundos.hpp"
#include <QObject>

class MainWindow;
class AppContext : public QObject {
	Q_OBJECT
	friend class MainWindow;

	/// Used to make the constructor private but accessible to make_shared
	struct private_tag {};

	QWidget* parent_widget;
	std::function<void(const fundos::db::outcome&)> on_fatal;

	std::shared_ptr<fundos::db> database;

	std::optional<fundos::currency_locale::selection>   currency;
	std::optional<fundos::percentage_locale::selection> percentage;

	std::vector<fundos::account> account_list;
	std::vector<fundos::fund>    fund_list;
	std::vector<fundos::budget>  budget_list;

	std::unordered_map<int64_t, fundos::account*> account_by_id;
	std::unordered_map<int64_t, fundos::fund*>    fund_by_id;
	std::unordered_map<int64_t, fundos::budget*>  budget_by_id;

	AppContext() = delete;
	AppContext(const AppContext&) = delete;
	AppContext& operator=(const AppContext&) = delete;

	struct creation_handles {
		std::function<void(std::shared_ptr<AppContext>)> on_success;
		std::function<void(std::shared_ptr<AppContext>)> on_needs_locale;
		std::function<void(const fundos::db::outcome&)>  on_fatal;
	};

	static void try_create(std::shared_ptr<fundos::db> database, QWidget* parent_widget, const creation_handles& handles);
	void initialize();
public:
	explicit AppContext(private_tag, std::function<void(const fundos::db::outcome&)> fatal_handler, QWidget* parent) : parent_widget(parent), on_fatal(fatal_handler) {}

	const std::shared_ptr<fundos::db>&          db()                const;
	const fundos::currency_locale::selection&   currency_locale()   const;
	const fundos::percentage_locale::selection& percentage_locale() const;

	const std::vector<fundos::account>& accounts() const;
	const std::vector<fundos::fund>&    funds()    const;
	const std::vector<fundos::budget>&  budgets()  const;

	const fundos::account* account(int64_t id) const;
	const fundos::fund*    fund   (int64_t id) const;

public slots:
	void refresh_locale();

	void refresh_accounts();
	void refresh_funds();
	void refresh_budgets();

	void update_account(const fundos::account& updating);
	void update_fund(const fundos::fund& updating);
	void update_budget(const fundos::budget& updating);

signals:
	void refreshed(std::shared_ptr<AppContext> ctx);
};
