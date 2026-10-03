#ifndef FUZZ_HELPER_HPP
#define FUZZ_HELPER_HPP

#include <cstdio>
#include <cstdlib>

#define FUZZ_CHECK(cond) \
	do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FUZZ_CHECK failed (%s:%d): %s\n", __FILE__, __LINE__, #cond); \
			std::abort(); \
		} \
	} while (0)

#endif
