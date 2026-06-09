#include "account_page.hpp"
#include "theme.hpp"
#include <QDateTime>
#include <QHboxLayout>
#include <QVboxLayout>
#include <QMessageBox>
#include <QSize>

AccountPage::AccountPage(std::shared_ptr<AppContext> ctx, fundos::account opening, QWidget *parent) : QWidget(parent), context(std::move(ctx)), record(std::move(opening)) {
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	{
		auto* header_row = new QWidget(this);
		auto* header_layout = new QHBoxLayout(header_row);
		header_layout->setContentsMargins(8, 8, 8, 8);
		header_layout->setSpacing(8);

		name_label = new EditableLabel(QString::fromStdString(record.name), this);
		connect(name_label, &EditableLabel::value_changed, this, &AccountPage::rename);
		QSize button_size = QSize(name_label->sizeHint().height(), name_label->sizeHint().height());

		auto* home_button = new QPushButton(this);
		home_button->setIcon(theme::colored_svg_icon(":/icons/home.svg", theme::text, button_size));
		home_button->setIconSize(button_size);
		home_button->setToolTip(tr("Home"));
		connect(home_button, &QPushButton::clicked, this, &AccountPage::go_home);

		auto* import_button = new QPushButton(tr("Import OFX"), this);
		import_button->setIcon(theme::colored_svg_icon(":/icons/upload.svg", theme::text, button_size));
		connect(import_button, &QPushButton::clicked, this, &AccountPage::import_ofx);

		auto* new_transaction_button = new QPushButton(tr("New Transaction"), this);
		new_transaction_button->setIcon(theme::colored_svg_icon(":/icons/plus.svg", theme::text, button_size));
		connect(new_transaction_button, &QPushButton::clicked, this, &AccountPage::new_transaction);

		close_button = new QPushButton(this);
		update_close_button();
		connect(close_button, &QPushButton::clicked, this, &AccountPage::on_toggle_open);

		header_layout->addWidget(home_button);
		header_layout->addWidget(name_label);
		header_layout->addStretch();
		header_layout->addWidget(import_button);
		header_layout->addWidget(new_transaction_button);
		header_layout->addWidget(close_button);

		layout->addWidget(header_row);
	}
	{
		auto* filter_row = new QWidget(this);
		auto* filter_layout = new QHBoxLayout(filter_row);
		filter_layout->setContentsMargins(8, 8, 8, 8);
		filter_layout->setSpacing(8);

		auto* after_label = new QLabel(tr("From"), this);
		auto* after_picker = new DatePicker(QDate::currentDate().addMonths(-1), this);

		auto* before_label = new QLabel(tr("Until"), this);
		before_picker = new DatePicker(QDate::currentDate(), this);

		QSize button_size = QSize(after_label->sizeHint().height(), after_label->sizeHint().height());

		auto* reload_button = new QPushButton(this);
		reload_button->setIcon(theme::colored_svg_icon(":/icons/reload.svg", theme::text, button_size));
		reload_button->setIconSize(button_size);
		reload_button->setToolTip(tr("Refresh"));
		connect(reload_button, &QPushButton::clicked, this, &AccountPage::refresh);

		filter_layout->addWidget(after_label);
		filter_layout->addWidget(after_picker);
		filter_layout->addSpacing(8);
		filter_layout->addWidget(before_label);
		filter_layout->addWidget(before_picker);
		filter_layout->addSpacing(8);
		filter_layout->addWidget(reload_button);
		filter_layout->addStretch();

		layout->addWidget(filter_row);
	}
	layout->addStretch();
}

void AccountPage::rename(QString name) {
	std::string old = record.name;
	record.name = name.toStdString();
	auto saved = context->db()->save_account(record);
	emit db_outcome(saved);
	if (!saved) {
		record.name = old;
		name_label->set_text(QString::fromStdString(old));
	} else {
		context->update_account(record);
	}
}

void AccountPage::new_transaction() {

}

void AccountPage::update_close_button() {
	QSize button_size = QSize(name_label->sizeHint().height(), name_label->sizeHint().height());
	if (record.closed_at.has_value()) {
		close_button->setText(tr("Open Account"));
		close_button->setIcon(theme::colored_svg_icon(":/icons/lock-open.svg", theme::success_foreground, button_size));
		close_button->setStyleSheet(QString(
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
		close_button->setText(tr("Close Account"));
		close_button->setIcon(theme::colored_svg_icon(":/icons/lock.svg", theme::error_foreground, button_size));
		close_button->setStyleSheet(QString(
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
void AccountPage::on_toggle_open() {
	auto old = record.closed_at;
	if (old.has_value()) {
		record.closed_at = std::nullopt;
	} else {
		record.closed_at = fundos::datetime{QDateTime::currentMSecsSinceEpoch()};
	}
	auto saved = context->db()->save_account(record);
	emit db_outcome(saved);
	if (!saved) {
		record.closed_at = old;
	} else {
		context->update_account(record);
	}
	update_close_button();
}

void AccountPage::refresh() {
	QMessageBox::information(this, "Opened", "Refreshing dates");
}
