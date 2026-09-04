#pragma once 

#include "../Match_Engine/MatchingEngineThread.h"
#include "../Lock_Free_Ring_Buffer/spsc/Ringbuffer.h"
#include "SnapshotWriter.h"

#include <thread>
#include <atomic>

namespace exchange {
class PersistenceThread {
public:
    explicit PersistenceThread (
        MatchingEngineThread& matching_thread, 
        SnapshotService& service, 
        SnapshotBinaryWriter& snapshot_writer, 
        EventBinaryWriter& event_writer
    ) : 
        matching_thread_(matching_thread), 
        service_(service), 
        snapshot_writer_(snapshot_writer), 
        event_writer_(event_writer) {}

    ~PersistenceThread() {
        stop();
    }
    
    void start() {
        if (running_) return;
        running_ = true;
        snapshot_writer_thread_ = std::thread(&PersistenceThread::snapshot_writer, this);
        event_writer_thread_ = std::thread(&PersistenceThread::events_writer, this);
        snapshot_thread_ = std::thread(&PersistenceThread::snapshot_loop, this);
    }

    void stop() {
        running_ = false;
        if (snapshot_writer_thread_.joinable()) snapshot_writer_thread_.join();
        if (event_writer_thread_.joinable()) event_writer_thread_.join();
        if (snapshot_thread_.joinable()) snapshot_thread_.join();
    }

private:
    SnapshotBinaryWriter& snapshot_writer_;
    SnapshotService& service_;
    EventBinaryWriter& event_writer_;
    MatchingEngineThread& matching_thread_;
    SPSCRingBuffer<SnapshotBatch> order_snapshot_{4};
    std::atomic<bool> running_{false};
    std::thread snapshot_writer_thread_;
    std::thread snapshot_thread_;
    std::thread event_writer_thread_;
    
    bool append_batch(uint64_t completed_sequence) {
        if (!service_.batch(completed_sequence).ready.load(std::memory_order_acquire)) return false;
        order_snapshot_.push(std::move(service_.batch(completed_sequence).batch));
        service_.reset_batch(completed_sequence);
        return true;
    }

    void events_writer() {
        while (running_) {
            event_writer_.write_events();
            std::this_thread::sleep_for(std::chrono::microseconds(100));            
        }
    }

    void snapshot_writer() {
        while (running_) {
            SnapshotBatch batch;
            while(order_snapshot_.pop(batch)) {
                if (batch.sequence > 9) std::filesystem::remove("snapshot" + std::to_string(batch.sequence - 10) + ".txt");
                snapshot_writer_.process_snapshot(batch);
            }
        }
    }

    void snapshot_loop() {        
        auto next_snapshot = std::chrono::system_clock::now() + std::chrono::seconds(60);

        while (running_) {
            matching_thread_.snapshot_requested();
            uint64_t completed = matching_thread_.completed_sequence();
            next_snapshot += std::chrono::seconds(60);
            while (!append_batch(completed)) std::this_thread::sleep_for(std::chrono::microseconds(100));
            std::this_thread::sleep_until(next_snapshot);
        }
    }
};
}