#include <catch2/catch_session.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

// See https://github.com/catchorg/Catch2/blob/v3.5.4/docs/own-main.md
int main(int argc, char* argv[]) {
#if defined(_WIN32) && defined(NDEBUG)
	// Guarantee the best performance and result stability across runs
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
#endif
	return Catch::Session().run(argc, argv);
}
