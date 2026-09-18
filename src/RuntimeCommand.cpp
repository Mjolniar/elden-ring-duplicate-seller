#include "RuntimeCommand.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace erdup {

namespace {

std::string trim(std::string value) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string stripInlineComment(std::string value) {
    const auto comment = value.find_first_of("#;");
    if (comment != std::string::npos) {
        value = value.substr(0, comment);
    }
    return trim(std::move(value));
}

std::optional<std::uint64_t> parseUnsigned64(const std::string& token) {
    try {
        std::size_t consumed = 0;
        const auto parsed = std::stoull(token, &consumed, 0);
        if (consumed != token.size()) {
            return std::nullopt;
        }
        return parsed;
    } catch (...) {
        return std::nullopt;
    }
}

std::string firstCommandLine(const std::string& text) {
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        line = stripInlineComment(std::move(line));
        if (!line.empty()) {
            return line;
        }
    }
    return {};
}

} // namespace

RuntimeCommand parseRuntimeCommandText(const std::string& text) {
    RuntimeCommand command;
    auto line = firstCommandLine(text);
    if (line.empty()) {
        return command;
    }

    command.raw = line;
    const auto equals = line.find('=');
    if (equals != std::string::npos) {
        const auto key = lower(trim(line.substr(0, equals)));
        if (key == "command" || key == "cmd" || key == "run") {
            line = trim(line.substr(equals + 1));
        }
    }

    std::istringstream parts(line);
    std::string token;
    parts >> token;
    token = lower(token);

    if (token == "inventory_preflight" || token == "preflight" ||
        token == "inventory_report" || token == "report" || token == "f8") {
        command.kind = RuntimeCommandKind::InventoryPreflight;
        return command;
    }
    if (token == "inventory_snapshot" || token == "snapshot") {
        command.kind = RuntimeCommandKind::InventorySnapshot;
        return command;
    }
    if (token == "sell_duplicates") {
        command.kind = RuntimeCommandKind::SellDuplicates;
        return command;
    }
    if (token == "field_dump" || token == "dump_fields") {
        command.kind = RuntimeCommandKind::FieldDump;
        return command;
    }
    if (token == "verify_chains" || token == "chain_verify") {
        command.kind = RuntimeCommandKind::VerifyChains;
        std::string valueToken;
        parts >> valueToken;
        const auto parsed = parseUnsigned64(valueToken);
        if (!parsed.has_value()) {
            command.error = "verify_chains requires a decimal or 0x-prefixed expected value";
            return command;
        }
        command.value = parsed.value();
        return command;
    }
    if (token == "help" || token == "?") {
        command.kind = RuntimeCommandKind::Help;
        return command;
    }

    command.kind = RuntimeCommandKind::Unknown;
    command.error = "unknown runtime command: " + token;
    return command;
}

std::string runtimeCommandKindToken(RuntimeCommandKind kind) {
    switch (kind) {
    case RuntimeCommandKind::None: return "none";
    case RuntimeCommandKind::InventoryPreflight: return "inventory_preflight";
    case RuntimeCommandKind::InventorySnapshot: return "inventory_snapshot";
    case RuntimeCommandKind::SellDuplicates: return "sell_duplicates";
    case RuntimeCommandKind::FieldDump: return "field_dump";
    case RuntimeCommandKind::VerifyChains: return "verify_chains";
    case RuntimeCommandKind::Help: return "help";
    case RuntimeCommandKind::Unknown: return "unknown";
    }
    return "unknown";
}

std::string formatRuntimeCommand(const RuntimeCommand& command) {
    std::ostringstream out;
    out << "runtime_command: kind=" << runtimeCommandKindToken(command.kind);
    if (command.value.has_value()) {
        out << " value=" << command.value.value();
    }
    if (!command.raw.empty()) {
        out << " raw=\"" << command.raw << "\"";
    }
    if (!command.error.empty()) {
        out << " error=\"" << command.error << "\"";
    }
    return out.str();
}

std::string runtimeCommandHelpText() {
    return "runtime_command commands: inventory_preflight, inventory_snapshot, sell_duplicates, field_dump, verify_chains <value>, help";
}

} // namespace erdup
