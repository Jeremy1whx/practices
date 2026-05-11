#include <atomic>
#include <vector>
#include <cassert>

template <typename T>
class RingBuffer {
    public:
        explicit RingBuffer(size_t capacity) : capacity_(capacity), buffer_(capacity) {
            assert (capacity & (capacity - 1) == 0 && "capacity must be power of 2");
            head_.store(0, std::memory_order_relaxed);
            tail_.store(0, std::memory_order_relaxed);
        }

        bool push(const T& item) {
            size_t head = head_.load(std::memory_order_relaxed);
            size_t next = (head + 1) & (capacity_ - 1);

            if (next == tail_.load(std::memory_order_acquire)) {
                return false;
            }

            buffer_[head] = item;
            head_.store(next, std::memory_order_release);
            return true;
        }

        bool pop(T& item) {
            size_t tail = tail_.load(std::memory_order_relaxed);

            if (tail == head_.load(std::memory_order_acquire)) {
                return false;
            }
            item = buffer_[tail];
            tail_.store((tail + 1) & (capacity_ - 1), std::memory_order_release);
            return true;
        }
    private:
        std::vector<T> buffer_;
        size_t capacity_;

        alignas(64) std::atomic<size_t> head_;
        alignas(64) std::atomic<size_t> tail_;
};