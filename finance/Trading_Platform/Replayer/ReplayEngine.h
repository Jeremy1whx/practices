#pragma once

#include "ReplayReader.h"
#include "../Match_Engine/MatchingEngineThread.h"
#include "../Risk_Engine/RiskMonitor.h"

namespace exchange {
class ReplayEngine {
public:
    ReplayEngine(BinaryReader& reader, MatchingEngine& engine, RiskEngine& risk_engine) : reader_(reader), engine_(engine), risk_engine_(risk_engine) {};

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
        risk_engine_.restore(batch.accounts, batch.last_trade_price);
        // for (const auto& acc : batch.accounts) {
        //     std::cout << "RESTORING ACC: id=" << acc.account_id 
        //               << ", pos=" << acc.position 
        //               << ", open_buy=" << acc.open_buy_quantity << std::endl;
        // }
        // std::cout << "[DEBUG] Read Snapshot: seq=" << batch.sequence 
        //           << ", orders=" << batch.orders.size() 
        //           << ", accounts=" << batch.accounts.size() << std::endl;
        engine_.restore_snapshot(batch.orders);
        reader_.close_snapshot();

        uint64_t event_seq = *latest_seq + 1;
        while (reader_.open_event_journal(event_seq)) {
            Event event;
            while (reader_.read_event(event)) {
                engine_.restore_events(event);
            }
            reader_.close_event();
            ++event_seq;
        }
        engine_.restore_schedule();
    };

private:
    BinaryReader& reader_;
    MatchingEngine& engine_;
    RiskEngine& risk_engine_;
};}