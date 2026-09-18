#include "DuplicateSeller.h"
#include "MutationSafety.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

erdup::ItemInstance item(
    std::uint32_t instanceId,
    std::uint32_t baseItemId,
    int sellValue,
    int quantity = 1
) {
    erdup::ItemInstance instance;
    instance.instanceId = instanceId;
    instance.baseItemId = baseItemId;
    instance.category = erdup::ItemCategory::Weapon;
    instance.quantity = quantity;
    instance.sellValue = sellValue;
    return instance;
}

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

bool hasBlocker(const erdup::MutationSafetyReport& report, erdup::MutationBlocker blocker) {
    for (const auto candidate : report.blockers) {
        if (candidate == blocker) {
            return true;
        }
    }
    return false;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

class FakeReadyAdapter final : public erdup::IGameAdapter {
public:
    explicit FakeReadyAdapter(std::vector<erdup::ItemInstance> initialInventory)
        : inventory(std::move(initialInventory)) {}

    bool isReady() const override {
        return true;
    }

    std::vector<erdup::ItemInstance> readInventory() override {
        return inventory;
    }

    bool removeItem(const erdup::ItemInstance&, int quantity) override {
        ++removeCalls;
        removedQuantity += quantity;
        return true;
    }

    bool addRunes(int amount) override {
        ++addRunesCalls;
        awardedRunes += amount;
        return true;
    }

    void showToast(const std::string& message) override {
        lastToast = message;
    }

    std::vector<erdup::ItemInstance> inventory;
    int removeCalls = 0;
    int removedQuantity = 0;
    int addRunesCalls = 0;
    int awardedRunes = 0;
    std::string lastToast;
};

int assertDryRunBlocksMutation() {
    erdup::Config config;
    config.dryRun = true;

    const auto plan = erdup::planDuplicateSale({
        item(1, 1000, 100),
        item(2, 1000, 100),
        item(3, 1000, 100),
    }, config);
    const auto report = erdup::evaluateMutationSafety(plan, config);

    if (report.allowed || !hasBlocker(report, erdup::MutationBlocker::DryRunEnabled)) {
        return fail("Mutation safety should block while dry_run is enabled.");
    }

    return EXIT_SUCCESS;
}

int assertEmptyPlanBlocksMutation() {
    erdup::Config config;
    config.dryRun = false;

    const auto plan = erdup::planDuplicateSale({}, config);
    const auto report = erdup::evaluateMutationSafety(plan, config);

    if (report.allowed || !hasBlocker(report, erdup::MutationBlocker::NoCandidates)) {
        return fail("Mutation safety should block an empty sale plan.");
    }

    return EXIT_SUCCESS;
}

int assertKnownPositiveSalePlanIsAllowed() {
    erdup::Config config;
    config.dryRun = false;

    const auto plan = erdup::planDuplicateSale({
        item(1, 1000, 100),
        item(2, 1000, 100),
        item(3, 1000, 100),
    }, config);
    const auto report = erdup::evaluateMutationSafety(plan, config);

    if (!report.allowed || !report.blockers.empty()) {
        return fail("Mutation safety should allow a known positive sale plan when dry_run is disabled.");
    }

    return EXIT_SUCCESS;
}

int assertUnknownSellValueBlocksMutation() {
    erdup::Config config;
    config.dryRun = false;

    const auto plan = erdup::planDuplicateSale({
        item(1, 1000, 0),
        item(2, 1000, 0),
        item(3, 1000, 0),
    }, config);
    const auto report = erdup::evaluateMutationSafety(plan, config);

    if (report.allowed ||
        !hasBlocker(report, erdup::MutationBlocker::UnknownSellValue) ||
        !hasBlocker(report, erdup::MutationBlocker::NonPositiveRuneTotal)) {
        return fail("Mutation safety should block unknown sell values before mutation.");
    }

    const auto formatted = erdup::formatMutationSafetyReport(report);
    if (!contains(formatted, "mutation preflight blocked") ||
        !contains(formatted, "unknown or non-positive sell value")) {
        return fail("Formatted mutation safety report should describe unknown sell values.");
    }

    return EXIT_SUCCESS;
}

int assertNonPositiveQuantityBlocksMutation() {
    erdup::Config config;
    config.dryRun = false;

    erdup::SellResult plan;
    erdup::SellCandidate candidate;
    candidate.item = item(1, 1000, 100);
    candidate.quantityToSell = 0;
    candidate.runeValue = 0;
    plan.candidates.push_back(candidate);
    plan.totalItems = 1;
    plan.totalRunes = 0;

    const auto report = erdup::evaluateMutationSafety(plan, config);
    if (report.allowed || !hasBlocker(report, erdup::MutationBlocker::NonPositiveQuantity)) {
        return fail("Mutation safety should block non-positive candidate quantities.");
    }

    return EXIT_SUCCESS;
}

int assertExecutorBlocksBeforeAdapterMutation() {
    erdup::Config config;
    config.dryRun = false;

    FakeReadyAdapter adapter({
        item(1, 1000, 0),
        item(2, 1000, 0),
        item(3, 1000, 0),
    });

    const auto result = erdup::executeDuplicateSale(adapter, config);
    if (!contains(result.message, "Mutation blocked") ||
        !contains(adapter.lastToast, "mutation preflight blocked")) {
        return fail("Executor should report mutation preflight failure.");
    }
    if (adapter.removeCalls != 0 || adapter.addRunesCalls != 0 ||
        adapter.removedQuantity != 0 || adapter.awardedRunes != 0) {
        return fail("Executor should not call adapter mutation methods when preflight fails.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertDryRunBlocksMutation() != EXIT_SUCCESS ||
        assertEmptyPlanBlocksMutation() != EXIT_SUCCESS ||
        assertKnownPositiveSalePlanIsAllowed() != EXIT_SUCCESS ||
        assertUnknownSellValueBlocksMutation() != EXIT_SUCCESS ||
        assertNonPositiveQuantityBlocksMutation() != EXIT_SUCCESS ||
        assertExecutorBlocksBeforeAdapterMutation() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Mutation safety test passed.\n";
    return EXIT_SUCCESS;
}
