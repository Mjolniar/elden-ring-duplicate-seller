#include "InventorySnapshotScheduler.h"

#include <chrono>
#include <cstdlib>
#include <iostream>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

int assertScheduleRequiresReadAndExport() {
    erdup::Config config;
    config.inventoryReadEnabled = true;
    config.inventorySnapshotExportEnabled = false;

    if (erdup::inventorySnapshotScheduleEnabled(config)) {
        return fail("Snapshot schedule should require snapshot export to be enabled.");
    }

    config.inventorySnapshotExportEnabled = true;
    config.inventorySnapshotAutoIntervalSeconds = 0;
    if (erdup::inventorySnapshotScheduleEnabled(config)) {
        return fail("Snapshot schedule should allow interval zero to disable automation.");
    }

    return EXIT_SUCCESS;
}

int assertRunOnStartThenCooldownAfterCompletion() {
    erdup::Config config;
    config.inventoryReadEnabled = true;
    config.inventorySnapshotExportEnabled = true;
    config.inventorySnapshotAutoIntervalSeconds = 5;
    config.inventorySnapshotAutoRunOnStart = true;

    erdup::InventorySnapshotScheduleState state;
    const auto start = std::chrono::steady_clock::time_point{};

    if (erdup::updateInventorySnapshotSchedule(state, config, start) !=
        erdup::InventorySnapshotScheduleAction::RunNow) {
        return fail("Run-on-start schedule should immediately request a snapshot.");
    }

    erdup::completeInventorySnapshotScheduleRun(state, config, start + std::chrono::seconds(2));
    if (erdup::updateInventorySnapshotSchedule(state, config, start + std::chrono::seconds(6)) !=
        erdup::InventorySnapshotScheduleAction::Waiting) {
        return fail("Schedule should wait until the cooldown after completion has elapsed.");
    }
    if (erdup::updateInventorySnapshotSchedule(state, config, start + std::chrono::seconds(7)) !=
        erdup::InventorySnapshotScheduleAction::RunNow) {
        return fail("Schedule should run once the cooldown after completion has elapsed.");
    }

    return EXIT_SUCCESS;
}

int assertDelayedFirstRunWhenConfigured() {
    erdup::Config config;
    config.inventoryReadEnabled = true;
    config.inventorySnapshotExportEnabled = true;
    config.inventorySnapshotAutoIntervalSeconds = 5;
    config.inventorySnapshotAutoRunOnStart = false;

    erdup::InventorySnapshotScheduleState state;
    const auto start = std::chrono::steady_clock::time_point{};

    if (erdup::updateInventorySnapshotSchedule(state, config, start) !=
        erdup::InventorySnapshotScheduleAction::Waiting) {
        return fail("Delayed schedule should wait on first poll.");
    }
    if (erdup::updateInventorySnapshotSchedule(state, config, start + std::chrono::seconds(4)) !=
        erdup::InventorySnapshotScheduleAction::Waiting) {
        return fail("Delayed schedule should keep waiting before the first interval.");
    }
    if (erdup::updateInventorySnapshotSchedule(state, config, start + std::chrono::seconds(5)) !=
        erdup::InventorySnapshotScheduleAction::RunNow) {
        return fail("Delayed schedule should run after the first interval.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertScheduleRequiresReadAndExport() != EXIT_SUCCESS ||
        assertRunOnStartThenCooldownAfterCompletion() != EXIT_SUCCESS ||
        assertDelayedFirstRunWhenConfigured() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Inventory snapshot scheduler test passed.\n";
    return EXIT_SUCCESS;
}
