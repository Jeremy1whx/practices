#pragma once

#include "ReplayReader.h"
#include "../Match_Engine/MatchingEngineThread.h"

namespace exchange {
class ReplayEngine {
public:
    ReplayEngine(BinaryReader& reader, EventBus& bus, MatchingEngine& engine) : reader_(reader), bus_(bus), engine_(engine) {};

    void replay() {

        auto latest_seq = reader_.get_latest_snapshot_sequence();
        if (!latest_seq) {
            std::cerr << "No snapshot found, cannot replay.\n";
            return;
        }

        if (!reader_.open_snapshot(latest_seq.value())) {
            std::cerr << "Failed to open snapshot " << latest_seq.value() << "\n";
            return;
        }
        SnapshotBatch batch;
        if (!reader_.read_snapshot(batch)) {
            std::cerr << "Failed to read snapshot.\n";
            return;
        }
        engine_.restore(batch.orders);
        reader_.close_snapshot();

        uint64_t event_seq = *latest_seq + 1;
        while (reader_.open_event_journal(event_seq)) {
            Event event;
            while (reader_.read_event(event)) {
                bus_.publish(event);
            }
            reader_.close_event();
            ++event_seq;
        }
    };

private:
    BinaryReader& reader_;
    EventBus& bus_;
    MatchingEngine& engine_;
};}