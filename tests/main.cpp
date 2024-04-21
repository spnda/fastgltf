#include <catch2/catch_session.hpp>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__) || defined(unix)
#include <sys/resource.h>
#include <unistd.h>
#endif

// See https://github.com/catchorg/Catch2/blob/v3.5.4/docs/own-main.md
int main(int argc, char* argv[]) {
	// Guarantee the best performance and result stability across runs
#if defined(_WIN32) && defined(NDEBUG)
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
#elif defined(__APPLE__) || defined(unix)
	setpriority(PRIO_PROCESS, getpid(), PRIO_MAX);
#endif
	return Catch::Session().run(argc, argv);
}
