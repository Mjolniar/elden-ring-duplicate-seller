#include "InventoryChangeAnalysis.h"
#include "InventorySnapshotFixture.h"

#include <iostream>

namespace {

void printUsage() {
    std::cerr << "Usage: InventoryChangeCli <before-snapshot.txt> <after-snapshot.txt>\n";
}

void printSnapshotMessages(
    const char* label,
    const erdup::InventorySnapshotFixtureResult& snapshot
) {
    for (const auto& warning : snapshot.warnings) {
        std::cerr << label << " warning: " << warning << "\n";
    }
    for (const auto& error : snapshot.errors) {
        std::cerr << label << " error: " << error << "\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        printUsage();
        return 2;
    }

    const auto before = erdup::loadInventorySnapshotFixture(argv[1]);
    const auto after = erdup::loadInventorySnapshotFixture(argv[2]);

    printSnapshotMessages("before", before);
    printSnapshotMessages("after", after);
    if (!before.errors.empty() || !after.errors.empty()) {
        return 1;
    }

    const auto analysis = erdup::analyzeInventoryChange(
        before.slots,
        before.activeCount,
        after.slots,
        after.activeCount
    );

    std::cout << erdup::formatInventoryChangeAnalysis(analysis) << "\n";
    for (const auto& warning : analysis.warnings) {
        std::cerr << "analysis warning: " << warning << "\n";
    }
    for (const auto& error : analysis.errors) {
        std::cerr << "analysis error: " << error << "\n";
    }

    return analysis.inputValid ? 0 : 1;
}
