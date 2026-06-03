#pragma once
#include <cstdio>
#include <cstdlib>

#ifdef NDEBUG
#define FUNDOS_ASSERT(condition, message) ((void)0)
#else
#define FUNDOS_ASSERT(condition, message) \
	do { if (!(condition)) { \
		std::fprintf(stderr, "Assertion failed: %s\n  %s\n  %s:%d\n", #condition, message, __FILE__, __LINE__); \
		std::fflush(stderr); \
		std::abort(); \
	}} while(0)
#endif

#if defined(_MSC_VER)
	#define FUNDOS_UNREACHABLE_IMPL() __assume(false)
#elif defined(__GNUC__) || defined(__clang__)
	#define FUNDOS_UNREACHABLE_IMPL() __builtin_unreachable()
#else
	#error "FUNDOS_UNREACHABLE_IMPL not implemented for this compiler"
#endif

#ifdef NDEBUG
	#define FUNDOS_UNREACHABLE() FUNDOS_UNREACHABLE_IMPL()
#else
	#define FUNDOS_UNREACHABLE() FUNDOS_ASSERT(false, "Unreachable code executed")
#endif
