#include <atomic>
#include <vector>
#include <cstddef>
#include <cassert>

template<typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : capacity_(capacity),
          buffer_(capacity)
    {
        assert((capacity & (capacity - 1)) == 0 && "capacity must be power of 2");

        for (auto& cell : buffer_) {
            cell.sequence.store(0, std::memory_order_relaxed);
        }

        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    bool push(const T& data) {
        size_t pos;

        while (true) {
            pos = head_.load(std::memory_order_relaxed);
            Cell& cell = buffer_[pos & (capacity_ - 1)];

            size_t seq = cell.sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)pos;

            if (diff == 0) {
                if (head_.compare_exchange_weak(
                        pos, pos + 1,
                        std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false; // full
            } else {
            }
        }

        Cell& cell = buffer_[pos & (capacity_ - 1)];
        cell.data = data;

        cell.sequence.store(pos + 1, std::memory_order_release);

        return true;
    }

    bool pop(T& data) {
        size_t pos = tail_.load(std::memory_order_relaxed);
        Cell& cell = buffer_[pos & (capacity_ - 1)];

        size_t seq = cell.sequence.load(std::memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);

        if (diff == 0) {
            data = cell.data;

            cell.sequence.store(pos + capacity_, std::memory_order_release);

            tail_.store(pos + 1, std::memory_order_relaxed);
            return true;
        } else if (diff < 0) {
            return false; // empty
        } else {
            return false;
        }
    }

private:
    struct Cell {
        std::atomic<size_t> sequence;
        T data;
    };

    std::vector<Cell> buffer_;
    size_t capacity_;

    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
};