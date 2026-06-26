#pragma once
#include "fundos.hpp"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QWidget>

class LocalePage : public QWidget {
	Q_OBJECT

	QComboBox*    currency_combo                    = nullptr;
	QComboBox*    percentage_combo                  = nullptr;

	QWidget*      currency_custom_fields            = nullptr;
	QComboBox*    currency_scale                    = nullptr;
	QLineEdit*    currency_symbol                   = nullptr;
	QLineEdit*    currency_thousands_separator      = nullptr;
	QLineEdit*    currency_decimal_separator        = nullptr;
	QButtonGroup* currency_symbol_placement_group   = nullptr;
	QButtonGroup* currency_negative_notation_group  = nullptr;
	QLabel*       currency_preview                  = nullptr;

	QWidget*      percentage_custom_fields          = nullptr;
	QLineEdit*    percentage_decimal_separator      = nullptr;
	QCheckBox*    percentage_has_space              = nullptr;
	QButtonGroup* percentage_symbol_placement_group = nullptr;
	QLabel*       percentage_preview                = nullptr;

	// Continuously synced from widget state via on_currency_preview/on_percentage_preview;
	// read in on_confirm() to build the final selection.
	fundos::currency_locale::spec   currency_fields;
	fundos::percentage_locale::spec percentage_fields;

	/// Holds the confirmed selection between on_confirm() and on_save_result(),
	/// so saved() can emit the correct values once the database acknowledges the write.
	std::optional<std::pair<fundos::currency_locale::selection, fundos::percentage_locale::selection>> pending_locales;

	/// @param can_cancel Cancel is only meaningful when locales already exist; first-time setup must commit.
	void setup_layout(bool can_cancel = false);

public:
	explicit LocalePage(
		std::optional<fundos::currency_locale::selection>   current_currency,
		std::optional<fundos::percentage_locale::selection> current_percentage,
		QWidget* parent = nullptr
	);

private slots:
	void on_confirm();
	void on_currency_preset();
	void on_percentage_preset();
	void on_currency_preview();
	void on_percentage_preview();

signals:
	void save_requested(fundos::currency_locale::selection currency, fundos::percentage_locale::selection percentage);
	void saved(fundos::currency_locale::selection currency, fundos::percentage_locale::selection percentage);
	void cancelled();

public slots:
	void on_save_result(fundos::db::outcome outcome);
};
