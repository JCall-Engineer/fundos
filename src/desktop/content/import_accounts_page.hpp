#pragma once
#include <cstdint>
#include <memory>
#include "coordinator.hpp"
#include "data/import.hpp"
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class ImportAccountsPage : public QWidget {
	Q_OBJECT

public:
	class Iterator {
		std::shared_ptr<fundos::import::pending_import> data;
		size_t current = 0;
	public:
		Iterator(std::shared_ptr<fundos::import::pending_import> data) : data(data) {}
		std::shared_ptr<fundos::import::pending_import> get() const { return data; }

		/// Skips past entries already matched in list, starting from the current position.
		/// Returns the first unmatched bank_account found, or nullptr if all entries are matched.
		/// Calling this again with the same list before current's entry is matched returns the same pointer.
		/// @param list the current set of known accounts to check matches against
		/// @return pointer to the next unmatched bank_account, or nullptr if none remain
		fundos::import::bank_account* advance_to_unmatched(const std::vector<fundos::account>& list) {
			for (; current < data->accounts.size(); ++current) {
				bool found = false;
				for (size_t i = 0; i < list.size(); ++i) {
					if (data->accounts[current].acct_id == list[i].bank_account_id) {
						found = true;
						break;
					}
				}
				if (!found) {
					return &data->accounts[current];
				}
			}
			return nullptr;
		}
	};

private:
	AppCoordinator*           app_coordinator;
	std::shared_ptr<Iterator> current;
	QVBoxLayout*              layout;
	QPushButton*              confirm_button;

	void show_current();

public:
	explicit ImportAccountsPage(AppCoordinator* coordinator, std::shared_ptr<Iterator> missing, QWidget* parent = nullptr);

private slots:
	void on_accounts_refreshed(fundos::db::result<fundos::account> results);
	void on_account_saved(fundos::db::outcome status);

signals:
	void refresh_accounts_requested();
	void save_account_requested(fundos::account saving);
	void ready_for_merge(std::shared_ptr<fundos::import::pending_import> data);
};
