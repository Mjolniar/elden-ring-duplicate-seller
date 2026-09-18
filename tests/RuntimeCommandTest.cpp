#include "RuntimeCommand.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int fail(const char* message) {
    std::cerr << message << "\n";
    return EXIT_FAILURE;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

int assertEmptyAndCommentOnlyIsNoOp() {
    const auto command = erdup::parseRuntimeCommandText("\n# comment\n; another comment\n");
    if (command.kind != erdup::RuntimeCommandKind::None ||
        erdup::runtimeCommandKindToken(command.kind) != "none") {
        return fail("Empty/comment-only runtime command text should parse as none.");
    }
    return EXIT_SUCCESS;
}

int assertInventoryPreflightAliasesParse() {
    const auto direct = erdup::parseRuntimeCommandText("inventory_preflight\n");
    const auto keyValue = erdup::parseRuntimeCommandText("command=report # trailing comment\n");
    const auto f8 = erdup::parseRuntimeCommandText("f8\n");
    if (direct.kind != erdup::RuntimeCommandKind::InventoryPreflight ||
        keyValue.kind != erdup::RuntimeCommandKind::InventoryPreflight ||
        f8.kind != erdup::RuntimeCommandKind::InventoryPreflight) {
        return fail("Inventory preflight aliases should parse consistently.");
    }
    return EXIT_SUCCESS;
}

int assertSnapshotAndFieldDumpParse() {
    const auto snapshot = erdup::parseRuntimeCommandText("run=snapshot\n");
    const auto fieldDump = erdup::parseRuntimeCommandText("field_dump\n");
    if (snapshot.kind != erdup::RuntimeCommandKind::InventorySnapshot ||
        fieldDump.kind != erdup::RuntimeCommandKind::FieldDump) {
        return fail("Snapshot and field dump runtime commands should parse.");
    }
    return EXIT_SUCCESS;
}

int assertVerifyChainsRequiresValue() {
    const auto valid = erdup::parseRuntimeCommandText("verify_chains 0x2a\n");
    if (valid.kind != erdup::RuntimeCommandKind::VerifyChains ||
        !valid.value.has_value() ||
        valid.value.value() != 42 ||
        !valid.error.empty()) {
        return fail("verify_chains should parse a 0x-prefixed expected value.");
    }

    const auto missing = erdup::parseRuntimeCommandText("verify_chains\n");
    if (missing.kind != erdup::RuntimeCommandKind::VerifyChains ||
        missing.error.empty()) {
        return fail("verify_chains without a value should produce an error.");
    }

    return EXIT_SUCCESS;
}

int assertUnknownAndFormattingAreStable() {
    const auto unknown = erdup::parseRuntimeCommandText("sell_everything\n");
    const auto formatted = erdup::formatRuntimeCommand(unknown);
    if (unknown.kind != erdup::RuntimeCommandKind::Unknown ||
        unknown.error.empty() ||
        !contains(formatted, "kind=unknown") ||
        !contains(formatted, "sell_everything")) {
        return fail("Unknown runtime commands should be rejected and formatted.");
    }
    if (!contains(erdup::runtimeCommandHelpText(), "inventory_preflight") ||
        !contains(erdup::runtimeCommandHelpText(), "verify_chains")) {
        return fail("Runtime command help text should mention supported commands.");
    }
    return EXIT_SUCCESS;
}

} // namespace

int main() {
    if (erdup::parseRuntimeCommandText("sell_duplicates").kind != erdup::RuntimeCommandKind::SellDuplicates)
        return fail("Explicit sale command must parse.");
    if (assertEmptyAndCommentOnlyIsNoOp() != EXIT_SUCCESS ||
        assertInventoryPreflightAliasesParse() != EXIT_SUCCESS ||
        assertSnapshotAndFieldDumpParse() != EXIT_SUCCESS ||
        assertVerifyChainsRequiresValue() != EXIT_SUCCESS ||
        assertUnknownAndFormattingAreStable() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    std::cout << "Runtime command test passed.\n";
    return EXIT_SUCCESS;
}
