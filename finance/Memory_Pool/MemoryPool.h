#pragma once

#include <vector>
#include <stack>
#include <cstddef>

template<typename T>
class MemoryPool {
public:

    explicit MemoryPool(size_t capacity) : storage_(capacity) {
        for (size_t i = 0; i < capacity; ++i) {
            free_list_.push(&storage_[i]);
        }
    }

    T* allocate() {
        if (free_list_.empty()) return nullptr;

        T* ptr = free_list_.top();

        free_list_.pop();

        return ptr;
    }

    void deallocate(T* ptr) {free_list_.push(ptr);}

    size_t available() const {return free_list_.size();}

private:
    std::vector<T> storage_;
    std::stack<T*> free_list_;
};