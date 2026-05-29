#include "currency_locale.hpp"

namespace fundos {

namespace currency_locale {

// Migrating from info having a std::string_view to std::string made registry have a nontrivial dtor
// As a consequence, locales cannot be a constexpr and must live in a compilation unit
// I'm quite unhappy about this, but admit defeat, this is the result of allowing custom locales
const registry locales = { .named = {} };

} // currency_locale

} // namespace fundos
