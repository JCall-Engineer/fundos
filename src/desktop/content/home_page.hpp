#pragma once
#include <functional>
#include "context.hpp"
#include <QWidget>
#include <QScrollArea>
#include <QString>

class HomePage : public QWidget {
	Q_OBJECT

	std::shared_ptr<AppContext> context;

	QScrollArea* scroll_area   = nullptr;
	QWidget*     account_panel = nullptr;
	QWidget*     fund_panel    = nullptr;
	QWidget*     budget_panel  = nullptr;

	struct button_spec {
		QString tooltip;
		QString icon_path;
		std::function<void()> action;

		QString checked_icon_path;
		void (HomePage::*toggle_signal)(bool) = nullptr;
	};

	QWidget* make_panel(QWidget* list, const QString& title, std::vector<button_spec> buttons);
	void relayout();

protected:
	void resizeEvent(QResizeEvent* event) override;

public:
	explicit HomePage(std::shared_ptr<AppContext> ctx, QWidget* parent = nullptr);

	///  Must be called after HomePage's signals are connected
	void initialize();

signals:
	void toggle_closed_accounts(bool);
	void toggle_closed_funds(bool);

	void db_outcome(const fundos::db::outcome& outcome);
	void open_account(const fundos::account& opening);
	void open_fund(const fundos::fund& opening);
	void open_budget(const fundos::budget& opening);
	void import_ofx();
	void refresh();
};
