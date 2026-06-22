#pragma once

#include "../Lock_Free_Ring_Buffer/mpsc/Ringbuffer.h"

template<typename T>
using MPSCQueue = RingBuffer<T>;