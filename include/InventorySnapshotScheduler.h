#pragma once

#include "Config.h"

#include <chrono>
#include <string>

namespace erdup {

enum class InventorySnapshotScheduleAction {
    Disabled,
    Waiting,
    RunNow,
};

struct InventorySnapshotScheduleState {
    bool initialized = false;
    std::chrono::steady_clock::time_point nextRun{};
};

bool inventorySnapshotScheduleEnabled(const Config& config);
std::chrono::seconds inventorySnapshotInterval(const Config& config);
InventorySnapshotScheduleAction updateInventorySnapshotSchedule(
    InventorySnapshotScheduleState& state,
    const Config& config,
    std::chrono::steady_clock::time_point now
);
void completeInventorySnapshotScheduleRun(
    InventorySnapshotScheduleState& state,
    const Config& config,
    std::chrono::steady_clock::time_point completedAt
);
std::string inventorySnapshotScheduleActionToken(InventorySnapshotScheduleAction action);

} // namespace erdup
