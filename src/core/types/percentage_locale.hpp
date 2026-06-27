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

/// The unit of exchange between the database and GUI for locale data:
/// the DB persists a selection and returns a selection, which the GUI consumes directly without needing to know which case it's in.
/// Wraps either a pointer to a known locale (so registry updates are reflected automatically) or a one-off custom spec.
/// identifier() doubles as the DB's serialization discriminator: a preset's identifier (e.g. "en") means look it up by name;
/// "Custom" means reconstruct the spec field-by-field from individual meta settings instead.
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

// Unlike currency_locale::registry, this can stay constexpr: percentage_locale::spec has no non-trivial members (no std::string), so the union's implicit destructor is fine as-is.
static constexpr registry locales = { .named = {} };

static inline const percentage_locale_entry* get_locale(std::string_view identifier) {
	for (std::size_t i = 0; i < num_locales; i++) {
		if (locales.entries[i].identifier == identifier)
			return &locales.entries[i];
	}
	return nullptr;
}

} // namespace fundos::percentage_locale
