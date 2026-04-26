#pragma once
#include <cstdint>
#include <string>

namespace fundos::percentage_locale {

struct info {
	enum class symbol_placement : uint8_t {
		before,
		after
	};

	char decimal_separator;
	bool has_space_around_number;
	symbol_placement symbol_position;
};

struct slot {
	const char* identifier;
	info info;
};

struct data {
	slot en {
		"en", {
			.decimal_separator = '.',
			.has_space_around_number = false,
			.symbol_position = info::symbol_placement::after,
		}
	};
	slot de {
		"de", {
			.decimal_separator = ',',
			.has_space_around_number = false,
			.symbol_position = info::symbol_placement::after,
		}
	};
	slot fr {
		"fr", {
			.decimal_separator = ',',
			.has_space_around_number = true,
			.symbol_position = info::symbol_placement::after,
		}
	};
};
static constexpr std::size_t num_locales = sizeof(data) / sizeof(slot);
static_assert(sizeof(data) % sizeof(slot) == 0, "slots must contain only slot members with no padding");

union registry {
	slot slots[num_locales];
	data named;
};
static constexpr registry locales = { .named = {} };

static inline std::optional<info> const get_locale(std::string_view identifier) {
	for (std::size_t i = 0; i < num_locales; i++) {
		if (locales.slots[i].identifier == identifier)
			return locales.slots[i].info;
	}
	return std::nullopt;
}

} // fundos::percentage_locale
