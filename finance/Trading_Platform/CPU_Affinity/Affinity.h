#pragma once

#include <thread>

#ifdef __linux__

#include <pthread.h>
#include <sched.h>

inline void pin_thread_to_core(std::thread& thread, int core_id) {

    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);

    CPU_SET(core_id, &cpuset);

    pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset);
}

#else

inline void pin_thread_to_core(std::thread&, int) {}

#endif