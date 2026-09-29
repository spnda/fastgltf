#include <iostream>

#include <catch2/catch_session.hpp>
#include "gltf_path.hpp"

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__) || defined(unix)
#include <sys/resource.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <pthread/qos.h>
#endif

// See https://github.com/catchorg/Catch2/blob/v3.5.4/docs/own-main.md
int main(int argc, char* argv[]) {
	if (!is_directory(sampleAssets) || is_empty(sampleAssets)) {
		std::cerr << "The sample models were not found. Please check the README for more information.\n" << std::endl;
		return -1;
	}

	// Guarantee the best performance and result stability across runs
#if defined(_WIN32) && defined(NDEBUG)
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
#elif defined(__APPLE__)
	// We can't pin cores explicitly on macOS, but we can set the highest QoS class to hopefully make the scheduler
	// pin us to P-cores.
	pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
	setpriority(PRIO_PROCESS, getpid(), PRIO_MIN);
#elif defined(unix)
	setpriority(PRIO_PROCESS, getpid(), PRIO_MIN);
#endif
	return Catch::Session().run(argc, argv);
}
