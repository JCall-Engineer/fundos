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
	// Prints condition, message, and source location to stderr, then aborts. This is the canonical
	// debug behavior that FUNDOS_UNREACHABLE, FUNDOS_REQUIRE, and FUNDOS_REQUIRE_OR_FALLBACK all rely on.
	#define FUNDOS_ASSERT(condition, message) \
		do { if (!(condition)) { \
			std::fprintf(stderr, "Assertion failed: %s\n  %s\n  %s:%d\n", #condition, message, __FILE__, __LINE__); \
			std::fflush(stderr); \
			std::abort(); \
		}} while(0)

	#define FUNDOS_UNREACHABLE() FUNDOS_ASSERT(false, "Unreachable code executed")
#endif

// For use in void functions: aborts in debug if condition is false; returns with no value in release.
// Note: the if-check below only runs in release builds, where FUNDOS_ASSERT is a no-op.
// In debug builds, FUNDOS_ASSERT already aborts on failure, so this check is unreachable there.
#define FUNDOS_REQUIRE(condition, message) \
	do { FUNDOS_ASSERT(condition, message); \
	     if (!(condition)) { return; } \
	} while(0)

// For use in value-returning functions: aborts in debug if condition is false; returns fallback in release.
#define FUNDOS_REQUIRE_OR_FALLBACK(condition, message, fallback) \
	do { FUNDOS_ASSERT(condition, message); \
	     if (!(condition)) { return fallback; } \
	} while(0)
