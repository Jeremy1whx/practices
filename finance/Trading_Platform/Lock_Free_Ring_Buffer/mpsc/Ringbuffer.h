#pragma once 

#include <atomic>
#include <vector>
#include <cstddef>
#include <cassert>

#include "CacheAligned.h"

template<typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity) : capacity_(capacity), buffer_(capacity) {
        assert((capacity & (capacity - 1)) == 0 && "capacity must be power of 2");
        head_.value.store(0, std::memory_order_relaxed);
        tail_.value.store(0, std::memory_order_relaxed);
        for (size_t i = 0; i < capacity; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    bool push(T& item) {
        while (true) {
            size_t pos = head_.value.load(std::memory_order_relaxed);
            size_t index = pos & (capacity_ - 1);
            size_t next = (pos + 1) & (capacity_ - 1);
            
            if (next == (tail_.value.load(std::memory_order_acquire) & (capacity_ - 1))) {
                return false;
            }
            
            Cell& cell = buffer_[index];
            size_t seq = cell.sequence.load(std::memory_order_acquire);
            
            if (seq == pos) {
                if (head_.value.compare_exchange_weak(pos, pos + 1, std::memory_order_release)) {
                    cell.data = std::move(item);
                    cell.sequence.store(pos + 1, std::memory_order_relaxed);
                    return true;
                }
            }
        }
    }

    bool push(const T& item) {
        T copy = item;
        return push(copy);
    }

    bool pop(T& item) {
        size_t tail = tail_.value.load(std::memory_order_relaxed);        
        if (tail == head_.value.load(std::memory_order_acquire)) return false; //empty
        size_t index = tail & (capacity_ - 1);
        Cell& cell = buffer_[index];
        size_t seq = cell.sequence.load(std::memory_order_acquire);
        if (seq == tail + 1) {
            item = cell.data;
            cell.sequence.store(seq + capacity_ - 1, std::memory_order_release);
            tail_.value.store(tail + 1, std::memory_order_release);
            return true;
        }
        return false;
        
    }

    bool empty() const {
        return head_.value.load(std::memory_order_acquire)
            == tail_.value.load(std::memory_order_acquire);
    }
    
private:
    struct Cell {
        std::atomic<size_t> sequence;
        T data;   
        Cell() : sequence(0), data() {}     
    };
    
    size_t capacity_;
    std::vector<Cell> buffer_;
    CacheAligned<std::atomic<size_t>> head_;
    CacheAligned<std::atomic<size_t>> tail_;
};