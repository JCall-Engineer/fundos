#pragma once
#include <cstdio>
#include <cstdlib>

#if defined(_MSC_VER)
	#define FUNDOS_UNREACHABLE_IMPL() __assume(false)
#elif defined(__GNUC__) || defined(__clang__)
	#define FUNDOS_UNREACHABLE_IMPL() __builtin_unreachable()
#else
	#error "FUNDOS_UNREACHABLE_IMPL not implemented for this compiler"
#endif

#ifdef NDEBUG
	// condition and message are not evaluated in release builds — never put side effects in condition.
	#define FUNDOS_ASSERT(condition, message) ((void)0)

	// Stronger and more dangerous than FUNDOS_ASSERT's release behavior: this isn't a no-op, it's a promise to the optimizer that this path is genuinely never reached.
	// If that promise is wrong, the compiler may miscompile surrounding code based on the false assumption, not just skip a check
	#define FUNDOS_UNREACHABLE() FUNDOS_UNREACHABLE_IMPL()
#else
	#define FUNDOS_ASSERT(condition, message) \
		do { if (!(condition)) { \
			std::fprintf(stderr, "Assertion failed: %s\n  %s\n  %s:%d\n", #condition, message, __FILE__, __LINE__); \
			std::fflush(stderr); \
			std::abort(); \
		}} while(0)

	#define FUNDOS_UNREACHABLE() FUNDOS_ASSERT(false, "Unreachable code executed")
#endif

#define FUNDOS_REQUIRE(condition, message) \
	do { FUNDOS_ASSERT(condition, message); \
	     if (!(condition)) { return; } \
	} while(0)

#define FUNDOS_REQUIRE_OR_FALLBACK(condition, message, fallback) \
	do { FUNDOS_ASSERT(condition, message); \
	     if (!(condition)) { return fallback; } \
	} while(0)
