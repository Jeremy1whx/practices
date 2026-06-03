#pragma once

#include "MemoryPool.h"

template <typename T>
class LRUPool : public MemoryPool<T>{
public:
    explicit LRUPool(size_t capacity) : MemoryPool<T>(capacity) {}

    T* allocate() override {
        if (this->free_list_.empty()) return nullptr;

        T* ptr = this->free_list_.top();

        this->free_list_.pop();

        ptr_list_.push_back(ptr);

        return ptr;
    }

    void recycle_oldest() {
        if (ptr_list_.empty()) return;
        T* ptr = ptr_list_.front();
        ptr_list_.pop_front();
        this->deallocate(ptr);
    }
private:
    std::deque<T*> ptr_list_;
};