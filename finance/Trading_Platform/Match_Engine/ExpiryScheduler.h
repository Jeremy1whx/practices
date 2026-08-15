#pragma once

#include <unordered_map>
#include <iostream>
#include <functional>

#include "../Order/OrderIngress.h"
#include "../Latency/Clock.h"
#include "../Memory_Pool/MemoryPool.h"
#include "../Event_Bus/CancelReason.h"

namespace exchange{
class ExpiryScheduler{
public:

    ExpiryScheduler() : beginning_(today_start_ns_utc()), last_processed_time_(beginning_) {}

    using CancelCallback = std::function<void(uint64_t, CancelReason)>;

    void set_cancel_callback(CancelCallback cb) {
        cancel_callback_ = std::move(cb);
    }

    void add_expiry(Order& order) {
        
        uint64_t expire_time = (order.expire_time - beginning_) / SECOND_NS;

        ExpiryEntry* entry = expiry_pool_.allocate();
        entry->expire_time = order.expire_time;
        entry->order_id = order.order_id;
        entry->next = nullptr;
        entry->prev = nullptr;

        if (expire_time <= 24 * 60 * 60) {
            size_t slot = expire_time - 1;
            append_to_list(seconds_wheel_[slot], entry);
        } else {
            size_t slot = ((expire_time / 86400) - 1 + day_offset_) % 366;
            append_to_list(days_wheel_[slot], entry);
        }        

        expiry_lookup_[entry->order_id] = entry;
    }

    bool delete_expiry(uint64_t order_id) {
        auto it = expiry_lookup_.find(order_id);

        if (it == expiry_lookup_.end()) {
            std::cerr << "delete_order: line " << 49 << " - Order not found:" << order_id << std::endl;
            return false;
        }

        ExpiryEntry* expire = it->second;

        uint64_t expire_time = (expire->expire_time - beginning_) / SECOND_NS;

        if (expire_time <= 24 * 60 * 60) {
            size_t slot = expire_time - 1;
            remove_from_list(seconds_wheel_[slot], expire);
        } else {
            size_t slot = ((expire_time / 86400) - 1 + day_offset_) % 366;
            remove_from_list(days_wheel_[slot], expire);
        }

        expiry_lookup_.erase(it);

        expiry_pool_.deallocate(expire);

        return true;
    }

    void proceed_expiry(uint64_t current_time) {

        uint64_t slot = ((current_time - beginning_) / SECOND_NS) - 1;
        uint64_t last_second = ((last_processed_time_ - beginning_) / SECOND_NS);

        if (last_second != slot) {

            if (slot >= 24 * 60 * 60) {
                proceed_wheel(last_second, 24 * 60 * 60, seconds_wheel_);
                
                auto elapse_days = (slot / (24 * 60 * 60));

                if (day_offset_ + elapse_days - 1 < 366) {
                    proceed_wheel(day_offset_, day_offset_ + elapse_days - 1, days_wheel_);                   
                } else {
                    proceed_wheel(day_offset_, 366, days_wheel_);
                    proceed_wheel(0, (day_offset_ + elapse_days - 1) % 366, days_wheel_);
                }
                
                day_offset_ = (day_offset_ + elapse_days - 1) % 366;

                beginning_ = today_start_ns_utc();    
                day_to_second(current_time);            
            } else {
                proceed_wheel(last_second, slot, seconds_wheel_);                
            }
            slot = slot % (24 * 60 * 60);
        }

        last_processed_time_ = current_time;

        if (!seconds_wheel_[slot].head) return;

        auto it = seconds_wheel_[slot].head;

        while (it) {
            auto* next = it->next;
            uint64_t order_id = it->order_id;
            delete_expiry(order_id);
            cancel_callback_(order_id, CancelReason::Expired);
            it = next;  
        } 

        if (slot == seconds_wheel_.size() - 1) {
            day_to_second(current_time);
            day_offset_ = (day_offset_ + 1) % 366;
            beginning_ += DAY_NS;
        }
    }

    size_t expiry_count() const {return expiry_lookup_.size();}

    bool contains(uint64_t order_id) const {
        return expiry_lookup_.find(order_id) != expiry_lookup_.end();
    }

    size_t seconds_entry_count() const {
        size_t count = 0;

        for (const auto& list : seconds_wheel_) {
            for (auto* it = list.head; it; it = it->next) {
                ++count;
            }
        }

        return count;
    }

    size_t days_entry_count() const {
        size_t count = 0;

        for (const auto& list : days_wheel_) {
            for (auto* it = list.head; it; it = it->next) {
                ++count;
            }
        }

        return count;
    }

    bool validate() const {
        size_t wheel_count = 0;

        for (const auto& list : seconds_wheel_) {
            for (auto* it = list.head; it; it = it->next) {
                ++wheel_count;

                auto lookup = expiry_lookup_.find(it->order_id);

                if (lookup == expiry_lookup_.end()) {
                    return false;
                }

                if (lookup->second != it) {
                    return false;
                }
            }
        }

        for (const auto& list : days_wheel_) {
            for (auto* it = list.head; it; it = it->next) {
                ++wheel_count;

                auto lookup = expiry_lookup_.find(it->order_id);

                if (lookup == expiry_lookup_.end()) {
                    return false;
                }

                if (lookup->second != it) {
                    return false;
                }
            }
        }

        return wheel_count == expiry_lookup_.size();
    }

private:

    struct ExpiryEntry{
        uint64_t expire_time;
        uint64_t order_id;
        ExpiryEntry* prev = nullptr;
        ExpiryEntry* next = nullptr;
    };

    struct ExpiryList {
        ExpiryEntry* head = nullptr;
        ExpiryEntry* tail = nullptr;
    };

    uint64_t beginning_; // today_start_ns_utc()
    uint64_t last_processed_time_; // = beginning_ at the start of running

    size_t day_offset_ = 0;

    std::vector<ExpiryList> days_wheel_{366};
    std::vector<ExpiryList> seconds_wheel_{24 * 60 * 60};

    std::unordered_map<uint64_t, ExpiryEntry*> expiry_lookup_;

    MemoryPool<ExpiryEntry> expiry_pool_{1024 * 1024};

    CancelCallback cancel_callback_;

    void append_to_list(ExpiryList& list, ExpiryEntry* entry) {
        entry->next = nullptr;
        entry->prev = list.tail;
        
        if (list.tail) {
            list.tail->next = entry;
        }
        list.tail = entry;

        if (!list.head) {
            list.head = entry;
        }
    }

    void remove_from_list(ExpiryList& list, ExpiryEntry* entry) {
        if (entry->prev) {
            entry->prev->next = entry->next;
        } else {
            list.head = entry->next;
        }

        if (entry->next) {
            entry->next->prev = entry->prev;
        } else {
            list.tail = entry->prev;
        }

        entry->prev = nullptr;
        entry->next = nullptr;
    }

    void day_to_second(uint64_t current_time) {
        if (!days_wheel_[day_offset_].head) return;

        auto* it = days_wheel_[day_offset_].head;
        while (it) {
            auto* next = it->next;
            if (it->expire_time <= current_time) {
                uint64_t order_id = it->order_id;
                auto expiry = expiry_lookup_.find(order_id);
                assert(expiry != expiry_lookup_.end());
                remove_from_list(days_wheel_[day_offset_], it);
                expiry_lookup_.erase(expiry);
                expiry_pool_.deallocate(it);
                cancel_callback_(order_id, CancelReason::Expired);

            } else {
                remove_from_list(days_wheel_[day_offset_], it);
                uint64_t expire_time = (it->expire_time - beginning_) / SECOND_NS;
                size_t slot = expire_time - 1;
                append_to_list(seconds_wheel_[slot], it);
            }
            it = next;
            
        }

        days_wheel_[day_offset_].head = nullptr;
        days_wheel_[day_offset_].tail = nullptr;
    }

    void proceed_wheel(size_t start, size_t end, std::vector<ExpiryList>& wheel) {

        for (size_t i = start; i < end; ++i) {

            if (!wheel[i].head) continue;
            auto it = wheel[i].head;
            while (it) {
                auto* next = it->next;
                uint64_t order_id = it->order_id;
                delete_expiry(order_id);
                cancel_callback_(order_id, CancelReason::Expired);
                it = next;
            }
        }
    }
};

}