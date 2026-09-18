#include "InventorySnapshotScheduler.h"

namespace erdup {

bool inventorySnapshotScheduleEnabled(const Config& config) {
    return config.inventoryReadEnabled &&
        config.inventorySnapshotExportEnabled &&
        config.inventorySnapshotAutoIntervalSeconds > 0;
}

std::chrono::seconds inventorySnapshotInterval(const Config& config) {
    return std::chrono::seconds(config.inventorySnapshotAutoIntervalSeconds);
}

InventorySnapshotScheduleAction updateInventorySnapshotSchedule(
    InventorySnapshotScheduleState& state,
    const Config& config,
    std::chrono::steady_clock::time_point now
) {
    if (!inventorySnapshotScheduleEnabled(config)) {
        state.initialized = false;
        return InventorySnapshotScheduleAction::Disabled;
    }

    const auto interval = inventorySnapshotInterval(config);
    if (!state.initialized) {
        state.initialized = true;
        state.nextRun = now + interval;
        if (config.inventorySnapshotAutoRunOnStart) {
            return InventorySnapshotScheduleAction::RunNow;
        }
        return InventorySnapshotScheduleAction::Waiting;
    }

    if (now < state.nextRun) {
        return InventorySnapshotScheduleAction::Waiting;
    }

    return InventorySnapshotScheduleAction::RunNow;
}

void completeInventorySnapshotScheduleRun(
    InventorySnapshotScheduleState& state,
    const Config& config,
    std::chrono::steady_clock::time_point completedAt
) {
    if (!inventorySnapshotScheduleEnabled(config)) {
        state.initialized = false;
        return;
    }

    state.initialized = true;
    state.nextRun = completedAt + inventorySnapshotInterval(config);
}

std::string inventorySnapshotScheduleActionToken(InventorySnapshotScheduleAction action) {
    switch (action) {
    case InventorySnapshotScheduleAction::Disabled:
        return "disabled";
    case InventorySnapshotScheduleAction::Waiting:
        return "waiting";
    case InventorySnapshotScheduleAction::RunNow:
        return "run_now";
    }
    return "unknown";
}

} // namespace erdup
