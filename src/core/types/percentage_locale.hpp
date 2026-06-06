#pragma once
#include <cstdint>
#include <string>
#include <variant>

namespace fundos::percentage_locale {

struct spec {
	enum class symbol_placement : uint8_t {
		before,
		after
	};

	char decimal_separator;
	bool has_space_around_number;
	symbol_placement symbol_position;
};

struct percentage_locale_entry {
	const char* identifier;
	spec info;
};

struct selection {
	std::variant<const percentage_locale_entry*, spec> raw;

	selection(const percentage_locale_entry* entry) : raw(entry)  {}
	selection(const spec& custom)                   : raw(custom) {}

	static constexpr const char* custom_id = "Custom";
	const char* identifier() const {
		if (std::holds_alternative<const percentage_locale_entry*>(raw)) {
			return std::get<const percentage_locale_entry*>(raw)->identifier;
		}
		return custom_id;
	 }
	 const spec& info() const {
		if (std::holds_alternative<const percentage_locale_entry*>(raw)) {
			return std::get<const percentage_locale_entry*>(raw)->info;
		}
		return std::get<spec>(raw);
	 }

	 bool is_preset() const { return std::holds_alternative<const percentage_locale_entry*>(raw); }
	 bool is_custom() const { return !is_preset(); }
};

struct data {
	percentage_locale_entry en {
		"en", {
			.decimal_separator = '.',
			.has_space_around_number = false,
			.symbol_position = spec::symbol_placement::after,
		}
	};
	percentage_locale_entry de {
		"de", {
			.decimal_separator = ',',
			.has_space_around_number = false,
			.symbol_position = spec::symbol_placement::after,
		}
	};
	percentage_locale_entry fr {
		"fr", {
			.decimal_separator = ',',
			.has_space_around_number = true,
			.symbol_position = spec::symbol_placement::after,
		}
	};
};
static constexpr std::size_t num_locales = sizeof(data) / sizeof(percentage_locale_entry);
static_assert(sizeof(data) % sizeof(percentage_locale_entry) == 0, "percentage locale data must contain only entry members with no padding");

union registry {
	percentage_locale_entry entries[num_locales];
	data named;
};
static constexpr registry locales = { .named = {} };

static inline const percentage_locale_entry* get_locale(std::string_view identifier) {
	for (std::size_t i = 0; i < num_locales; i++) {
		if (locales.entries[i].identifier == identifier)
			return &locales.entries[i];
	}
	return nullptr;
}

} // namespace fundos::percentage_locale
