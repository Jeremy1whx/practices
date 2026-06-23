#pragma once

template<typename T>
struct alignas(64) CacheAligned {
    T value;
};