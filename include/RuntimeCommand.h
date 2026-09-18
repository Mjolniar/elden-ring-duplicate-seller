#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace erdup {

enum class RuntimeCommandKind {
    None,
    InventoryPreflight,
    InventorySnapshot,
    SellDuplicates,
    FieldDump,
    VerifyChains,
    Help,
    Unknown,
};

struct RuntimeCommand {
    RuntimeCommandKind kind = RuntimeCommandKind::None;
    std::optional<std::uint64_t> value;
    std::string raw;
    std::string error;
};

RuntimeCommand parseRuntimeCommandText(const std::string& text);
std::string runtimeCommandKindToken(RuntimeCommandKind kind);
std::string formatRuntimeCommand(const RuntimeCommand& command);
std::string runtimeCommandHelpText();

} // namespace erdup
