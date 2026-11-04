#pragma once

#include <cstdint>
#include <vector>
#include <limits>

namespace exchange {

class OrderLookupTable {
public:
    struct Entry {
        uint64_t account_id;
        int64_t  credit_per_unit;
    };

    explicit OrderLookupTable(size_t min_capacity) {
        size_t cap = next_pow2(min_capacity);
        capacity_ = cap;
        mask_ = cap - 1;
        uint32_t bits = 0;
        while ((size_t(1) << bits) < cap) ++bits;
        shift_ = 64 - bits;
        slots_.assign(cap, Slot{});
        size_ = 0;
        tombstone_ = 0;
    }

    [[nodiscard]] const Entry* find(uint64_t order_id) const {
        size_t idx = hash_index(order_id);
        for (size_t probe = 0; probe < capacity_; ++probe) {
            const Slot& s = slots_[idx];
            if (s.state == EMPTY) return nullptr;
            if (s.state == OCCUPIED && s.order_id == order_id) {
                return reinterpret_cast<const Entry*>(&s.account_id);
            }
            idx = (idx + 1) & mask_;
        }
        return nullptr;
    }


    bool insert(uint64_t order_id, uint64_t account_id, int64_t cpu) {
        if (order_id == 0) return false;

        if ((size_ + tombstone_) * 10 >= capacity_ * 7) {
            if (size_ * 10 >= capacity_ * 7) {rebuild(capacity_ * 2);}
            else rebuild(capacity_);
        }

        return insert_no_rehash(order_id, account_id, cpu);
    }
    

    bool erase(uint64_t order_id) {
        size_t idx = hash_index(order_id);
        for (size_t probe = 0; probe < capacity_; ++probe) {
            Slot& s = slots_[idx];
            if (s.state == EMPTY) return false;
            if (s.state == OCCUPIED && s.order_id == order_id) {
                s.state = TOMBSTONE;
                ++tombstone_;
                --size_;
                return true;
            }
            idx = (idx + 1) & mask_;
        }
        return false;
    }

    void clear() {
        for (auto& s : slots_) s.state = EMPTY;
        size_ = 0;
        tombstone_ = 0;
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }
    size_t tombstone() const { return tombstone_; }

private:
    static constexpr uint32_t EMPTY = 0;
    static constexpr uint32_t OCCUPIED = 1;
    static constexpr uint32_t TOMBSTONE = 2;

    struct Slot {
        uint64_t order_id;
        uint64_t account_id;
        int64_t credit_per_unit;
        uint32_t state;
        uint32_t _pad;
    };

    [[nodiscard]] size_t hash_index(uint64_t id) const {
        return static_cast<size_t>((id * 0x9E3779B97F4A7C15ULL) >> shift_);
    }

    bool insert_no_rehash(uint64_t order_id, uint64_t account_id, int64_t cpu) {
        if (order_id == 0) return false;
        size_t idx = hash_index(order_id);
        size_t first_tombstone = SIZE_MAX;

        for (size_t probe = 0; probe < capacity_; ++probe) {
            Slot& s = slots_[idx];

            if (s.state == EMPTY) {
                size_t target = (first_tombstone != SIZE_MAX) ? first_tombstone : idx;
                Slot& t = slots_[target];
                if (first_tombstone != SIZE_MAX) --tombstone_;
                t.order_id = order_id;
                t.account_id = account_id;
                t.credit_per_unit = cpu;
                t.state = OCCUPIED;
                ++size_;
                return true;
            }

            if (s.state == TOMBSTONE) {
                if (first_tombstone == SIZE_MAX) first_tombstone = idx;
            } else if (s.order_id == order_id) {
                s.account_id = account_id;
                s.credit_per_unit = cpu;
                return true;
            }

            idx = (idx + 1) & mask_;
        }
        return false;
    }

    static size_t next_pow2(size_t x) {
        if (x < 2) return 2;
        size_t cap = 1;
        while (cap < x) cap <<= 1;
        return cap;
    }
    
    void rebuild(size_t new_capacity) {
        size_t cap = next_pow2(new_capacity);
        while (size_ * 10 > cap * 7) cap <<= 1;

        std::vector<Slot> fresh(cap);
        for (auto& s : fresh) s.state = EMPTY;
        std::swap(slots_, fresh); 

        capacity_ = cap;
        mask_ = cap - 1;
        uint32_t bits = 0;
        while ((size_t(1) << bits) < cap) ++bits;
        shift_ = 64 - bits;
        size_ = 0;
        tombstone_ = 0;
        for (const Slot& s : fresh) {
            if (s.state == OCCUPIED && s.order_id != 0) {
                insert_no_rehash(s.order_id, s.account_id, s.credit_per_unit);
            }
        }
    }

    std::vector<Slot> slots_;
    size_t capacity_ = 0;
    size_t mask_ = 0;
    size_t size_ = 0;
    size_t tombstone_ = 0;
    uint32_t shift_ = 0;
};
} 