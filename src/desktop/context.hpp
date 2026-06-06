#pragma once
#include "fundos.hpp"

struct AppContext {
	std::shared_ptr<fundos::db>          database;
	fundos::currency_locale::selection   currency_locale;
	fundos::percentage_locale::selection percentage_locale;

	AppContext(
		std::shared_ptr<fundos::db>          db,
		fundos::currency_locale::selection   currency,
		fundos::percentage_locale::selection percentage
	) : database(std::move(db)), currency_locale(std::move(currency)), percentage_locale(std::move(percentage)) {}
};
