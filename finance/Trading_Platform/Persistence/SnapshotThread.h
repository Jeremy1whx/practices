#pragma once 

#include "../Match_Engine/MatchingEngineThread.h"
#include "../Lock_Free_Ring_Buffer/spsc/Ringbuffer.h"
#include "SnapshotWriter.h"

#include <thread>
#include <atomic>

namespace exchange {
class SnapshotThread {
public:
    explicit SnapshotThread (MatchingEngineThread& matching_thread, SnapshotService& service, SnapshotTextWriter& writer) : matching_thread_(matching_thread), service_(service), writer_(writer) {}

    ~SnapshotThread() {
        stop();
    }
    
    void start() {
        if (running_) return;
        running_ = true;
        writer_thread_ = std::thread(&SnapshotThread::run, this);
        snapshot_thread_ = std::thread(&SnapshotThread::snapshot_loop, this);
    }

    void stop() {
        running_ = false;
        if (writer_thread_.joinable()) writer_thread_.join();
        if (snapshot_thread_.joinable()) snapshot_thread_.join();
    }

private:
    SnapshotTextWriter& writer_;
    SnapshotService& service_;
    MatchingEngineThread& matching_thread_;
    SPSCRingBuffer<SnapshotBatch> order_snapshot_{4};
    std::atomic<bool> running_{false};
    std::thread writer_thread_;
    std::thread snapshot_thread_;
    
    bool append_batch(uint64_t completed_sequence) {
        if (!service_.batch(completed_sequence).ready.load(std::memory_order_acquire)) return false;
        order_snapshot_.push(std::move(service_.batch(completed_sequence).batch));
        service_.reset_batch(completed_sequence);
        return true;
    }

    void run() {
        while (running_) {
            SnapshotBatch batch;
            while(order_snapshot_.pop(batch)) {
                writer_.process_snapshot(batch);
            }
        }
    }

    void snapshot_loop() {        
        auto next_snapshot = std::chrono::system_clock::now() + std::chrono::seconds(60);

        while (running_) {
            matching_thread_.snapshot_requested();
            uint64_t completed = matching_thread_.completed_sequence();
            next_snapshot += std::chrono::seconds(60);
            while (!append_batch(completed)) std::this_thread::yield();
            std::this_thread::sleep_until(next_snapshot);
        }
    }
};
}