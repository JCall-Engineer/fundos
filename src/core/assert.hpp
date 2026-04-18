#pragma once
#include <cstdio>
#include <cstdlib>

#ifdef NDEBUG
#define FUNDOS_ASSERT(condition, message) ((void)0)
#else
#define FUNDOS_ASSERT(condition, message) \
	do { if (!(condition)) { \
		std::fprintf(stderr, "Assertion failed: %s\n  %s\n  %s:%d\n", #condition, message, __FILE__, __LINE__); \
		std::abort(); \
	}} while(0)
#endif
