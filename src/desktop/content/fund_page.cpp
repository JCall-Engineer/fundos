#include "fund_page.hpp"
#include "theme.hpp"
#include "components/loading_spinner.hpp"
#include <QDateTime>
#include <QHBoxLayout>
#include <QScrollArea>

FundPage::FundPage(
	AppCoordinator* coordinator,
	fundos::fund opening,
	QWidget *parent
) : QWidget(parent), app_coordinator(std::move(coordinator)), record(std::move(opening)) {
	auto* database = app_coordinator->database();
	connect(this,     &FundPage::save_fund_requested,      database, &AppDatabase::save_fund);
	connect(this,     &FundPage::history_requested,        database, &AppDatabase::request_fund_history);
	connect(database, &AppDatabase::fund_saved,            this,     &FundPage::on_fund_saved);
	connect(database, &AppDatabase::fund_history_received, this,     &FundPage::on_history);

	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	{
		auto* header_row = new QWidget(this);
		header_row->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

		auto* header_layout = new QHBoxLayout(header_row);
		header_layout->setContentsMargins(8, 8, 8, 8);
		header_layout->setSpacing(8);

		name_label = new EditableLabel(QString::fromStdString(record.name), this);
		connect(name_label, &EditableLabel::value_changed, this, &FundPage::rename);

		auto* home_button = new QPushButton(this);
		home_button->setIcon(theme::colored_svg_icon(":/icons/home.svg", theme::text, theme::toolbar_icon_size));
		home_button->setIconSize(theme::label_icon_size(name_label));
		home_button->setToolTip(tr("Home"));
		connect(home_button, &QPushButton::clicked, this, &FundPage::go_home);

		open_close_button = new QPushButton(this);
		update_open_close_button();
		connect(open_close_button, &QPushButton::clicked, this, &FundPage::on_toggle_open);

		header_layout->addWidget(home_button);
		header_layout->addWidget(name_label);
		header_layout->addStretch();
		header_layout->addWidget(open_close_button);

		layout->addWidget(header_row);
	}
	{
		auto* filter_row = new QWidget(this);
		filter_row->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

		auto* filter_layout = new QHBoxLayout(filter_row);
		filter_layout->setContentsMargins(8, 8, 8, 8);
		filter_layout->setSpacing(8);

		auto today = QDate::currentDate();
		auto* after_label = new QLabel(tr("From"), this);
		after_picker = new DatePicker(today.addMonths(-1), this);

		auto* before_label = new QLabel(tr("Until"), this);
		before_picker = new DatePicker(today, this);

		auto* month_button = new QPushButton(tr("Last Month"), this);
		connect(month_button, &QPushButton::clicked, this, [this]() {
			loading_preset_date_range = true;
			auto today = QDate::currentDate();
			after_picker->set_date(today.addMonths(-1));
			before_picker->set_date(today);
			loading_preset_date_range = false;
			fetch_history();
		});

		auto* quarter_button = new QPushButton(tr("3 Months"), this);
		connect(quarter_button, &QPushButton::clicked, this, [this]() {
			loading_preset_date_range = true;
			auto today = QDate::currentDate();
			after_picker->set_date(today.addMonths(-3));
			before_picker->set_date(today);
			loading_preset_date_range = false;
			fetch_history();
		});

		auto* ytd_button = new QPushButton(tr("YTD"), this);
		connect(ytd_button, &QPushButton::clicked, this, [this]() {
			loading_preset_date_range = true;
			auto today = QDate::currentDate();
			after_picker->set_date(QDate(today.year(), 1, 1));
			before_picker->set_date(today);
			loading_preset_date_range = false;
			fetch_history();
		});

		filter_layout->addWidget(after_label);
		filter_layout->addWidget(after_picker);
		filter_layout->addSpacing(8);
		filter_layout->addWidget(before_label);
		filter_layout->addWidget(before_picker);
		filter_layout->addSpacing(8);
		filter_layout->addWidget(month_button);
		filter_layout->addWidget(quarter_button);
		filter_layout->addWidget(ytd_button);
		filter_layout->addStretch();

		layout->addWidget(filter_row);
	}
	{
		auto* history_scroll = new QScrollArea(this);
		history_scroll->setWidgetResizable(true);

		history_panel = new QWidget();
		history_scroll->setWidget(history_panel);

		history_layout = new QVBoxLayout(history_panel);
		history_layout->setContentsMargins(0, 0, 0, 0);
		history_layout->setSpacing(0);
		fetch_history();

		layout->addWidget(history_scroll, 1);
	}
}

void FundPage::rename(QString name) {
	previous_name = record.name;
	previous_closed_at = record.closed_at;
	record.name = name.toStdString();
	emit save_fund_requested(record);
}

void FundPage::update_open_close_button() {
	if (record.closed_at.has_value()) {
		open_close_button->setText(tr("Open Fund"));
		open_close_button->setIcon(theme::colored_svg_icon(":/icons/lock-open.svg", theme::success_foreground, theme::toolbar_icon_size));
		open_close_button->setStyleSheet(QString(
			"QPushButton {"
			"  color: %1;"
			"  border: 1px solid %1;"
			"  border-radius: 4px;"
			"  padding: 4px 8px;"
			"}"
			"QPushButton:hover {"
			"  color: %2;"
			"  border-color: %2;"
			"}"
		).arg(
			theme::success_foreground.name(),
			theme::success_foreground.lighter(120).name()
		));
	} else {
		open_close_button->setText(tr("Close Fund"));
		open_close_button->setIcon(theme::colored_svg_icon(":/icons/lock.svg", theme::error_foreground, theme::toolbar_icon_size));
		open_close_button->setStyleSheet(QString(
			"QPushButton {"
			"  color: %1;"
			"  border: 1px solid %1;"
			"  border-radius: 4px;"
			"  padding: 4px 8px;"
			"}"
			"QPushButton:hover {"
			"  color: %2;"
			"  border-color: %2;"
			"}"
		).arg(
			theme::error_foreground.name(),
			theme::error_foreground.lighter(120).name()
		));
	}
}
void FundPage::on_toggle_open() {
	previous_name = record.name;
	previous_closed_at = record.closed_at;
	if (previous_closed_at.has_value()) {
		record.closed_at = std::nullopt;
	} else {
		record.closed_at = fundos::datetime{QDateTime::currentMSecsSinceEpoch()};
	}
	emit save_fund_requested(record);
}

void FundPage::on_fund_saved(fundos::db::outcome saved) {
	if (!saved) {
		record.name = previous_name;
		name_label->set_text(QString::fromStdString(previous_name));
		record.closed_at = previous_closed_at;
	} else {
		app_coordinator->update_fund(record);
	}
	update_open_close_button();
}

void FundPage::clear_history() {
	while (QLayoutItem* item = history_layout->takeAt(0)) {
		delete item->widget();
		delete item;
	}
}

void FundPage::fetch_history() {
	if (loading_preset_date_range) { return; }
	clear_history();

	auto* info_row = new QWidget(history_panel);
	history_layout->addWidget(info_row);

	auto* info_layout = new QHBoxLayout(info_row);

	auto* spinner = new LoadingSpinner(this);
	info_layout->addWidget(spinner);

	fundos::datetime after  = {after_picker->date().startOfDay().toMSecsSinceEpoch()};
	fundos::datetime before = {before_picker->date().endOfDay().toMSecsSinceEpoch()};
	emit history_requested(record.id(), after, before);
}

void FundPage::on_history(fundos::db::result<fundos::db::allocation_history> received) {
	clear_history();
	if (!received) {
		auto* info_row = new QWidget(this);
		history_layout->addWidget(info_row);

		auto* info_layout = new QHBoxLayout(info_row);
		info_layout->addWidget(theme::header_label(tr("Error getting transaction history."), history_panel), 0, Qt::AlignCenter);
		return;
	}
	auto& history = received.value();
	if (history.transactions.empty()) {
		auto* info_row = new QWidget(this);
		history_layout->addWidget(info_row);

		auto* info_layout = new QHBoxLayout(info_row);
		info_layout->addWidget(theme::header_label(tr("No transactions recorded during the selected period."), history_panel), 0, Qt::AlignCenter);
		return;
	}
	for (auto& transaction : history.transactions) {

	}
	history_layout->addStretch();
}
