#include "InventorySnapshotFixture.h"

#include <cstdlib>
#include <iostream>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

int assertParsesCountSlotsAndGaps() {
    const auto snapshot = erdup::parseInventorySnapshotFixture(R"(
count=3
slot=index:0,handle:0x100,id:0x1234,category:weapon,qty:1
slot=index:2,empty:true
)");

    if (!snapshot.errors.empty() ||
        snapshot.activeCount != 3 ||
        !snapshot.activeCountExplicit ||
        snapshot.slots.size() != 3 ||
        snapshot.slots[0].rawItemId != erdup::rawInventoryItemId(erdup::ItemCategory::Weapon, 0x1234) ||
        snapshot.slots[1].rawItemId != 0xffffffffu ||
        snapshot.slots[2].rawItemId != 0xffffffffu ||
        snapshot.warnings.empty()) {
        return fail("Snapshot fixture should parse count, insert gaps, and keep empty rows.");
    }

    return EXIT_SUCCESS;
}

int assertParsesRawItemIdAndImplicitCount() {
    const auto snapshot = erdup::parseInventorySnapshotFixture(R"(
slot=handle:0x200,raw:0x80001234,qty:1
slot=handle:0x201,raw:0x20005678,qty:2,display:9
)");

    if (!snapshot.errors.empty() ||
        snapshot.activeCount != 2 ||
        snapshot.activeCountExplicit ||
        snapshot.slots.size() != 2 ||
        snapshot.slots[0].rawItemId != 0x80001234u ||
        snapshot.slots[1].displayIndex != 9) {
        return fail("Snapshot fixture should parse raw item ids and implicit count.");
    }

    return EXIT_SUCCESS;
}

int assertInvalidRowsBecomeErrors() {
    const auto snapshot = erdup::parseInventorySnapshotFixture(R"(
count=not_a_number
slot=handle:0x300,id:0x1234,category:not_real,qty:1
slot=index:0,handle:0x301,qty:1
)");

    if (snapshot.errors.size() != 3) {
        return fail("Invalid snapshot rows should produce errors.");
    }

    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (assertParsesCountSlotsAndGaps() != EXIT_SUCCESS ||
        assertParsesRawItemIdAndImplicitCount() != EXIT_SUCCESS ||
        assertInvalidRowsBecomeErrors() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Inventory snapshot fixture test passed.\n";
    return EXIT_SUCCESS;
}
