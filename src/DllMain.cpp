#include "Config.h"
#include "CompactRowDiagnostics.h"
#include "DuplicateSeller.h"
#include "FieldDump.h"
#include "GameAdapter.h"
#include "HandleDiagnostics.h"
#include "InventoryReader.h"
#include "InventoryReport.h"
#include "InventorySnapshotRetention.h"
#include "InventorySnapshotScheduler.h"
#include "ItemMetadata.h"
#include "LiveMutationGate.h"
#include "Logger.h"
#include "MemoryScanner.h"
#include "MutationSafety.h"
#include "NativeSeller.h"
#include "MutationRoutineLoader.h"
#include "MutationRoutineRegistry.h"
#include "MutationRoutineResolver.h"
#include "PointerScanner.h"
#include "ReadOnlyProbe.h"
#include "RuntimeCommand.h"
#include "RuntimeDiagnostics.h"
#include "RuntimeGuards.h"
#include "SellValueMetadata.h"
#include "SnapshotSalePlan.h"
#include "WatchedFile.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {

std::atomic<bool> g_running = true;
HMODULE g_module = nullptr;
constexpr std::size_t kMaxLoggedInventoryReadSkips = 16;

erdup::SellResult sellResultFromReportForMutationPreflight(const erdup::InventoryReport& report) {
    erdup::SellResult result;
    for (const auto& row : report.rows) {
        if (!row.selectedForSale || row.quantityToSell <= 0) {
            continue;
        }

        erdup::SellCandidate candidate;
        candidate.item = row.item;
        if (row.sellValueResolution != erdup::SellValueResolution::Resolved) {
            candidate.item.sellValue = 0;
        }
        candidate.quantityToSell = row.quantityToSell;
        candidate.runeValue = row.runeValue;
        result.totalItems += row.quantityToSell;
        result.totalRunes += row.runeValue;
        result.candidates.push_back(candidate);
    }

    std::ostringstream message;
    message << "Planned " << result.totalItems << " duplicate item sale(s) for "
        << result.totalRunes << " rune(s).";
    result.message = message.str();
    return result;
}

std::filesystem::path moduleDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(g_module, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

std::string readTextFile(const std::filesystem::path& path, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "could not open " + path.string();
        return {};
    }

    std::ostringstream text;
    text << file.rdbuf();
    if (!file.good() && !file.eof()) {
        error = "could not read " + path.string();
        return {};
    }

    error.clear();
    return text.str();
}

std::filesystem::path nextInventorySnapshotPath(const std::filesystem::path& directory);

void workerThread() {
    const auto dllDirectory = moduleDirectory();
    erdup::initializeLogger(dllDirectory / "ERDuplicateSeller.log");

    const auto configPath = dllDirectory / "duplicate_seller.ini";
    const auto memoryScanTargetPath = dllDirectory / "memory_scan_target.txt";
    const auto runtimeCommandPath = dllDirectory / "runtime_command.txt";
    auto config = erdup::loadConfig(configPath);
    erdup::logInfo("ERDuplicateSeller loaded: " + erdup::describeConfig(config));

    const auto metadataLoad = erdup::loadConfiguredMetadata(config);
    erdup::MetadataIndex metadata;
    metadata.add(metadataLoad);
    erdup::logInfo(
        "Metadata loaded: items=" + std::to_string(metadataLoad.items.size()) +
        ", index_entries=" + std::to_string(metadata.size()) +
        ", warnings=" + std::to_string(metadata.warnings().size())
    );
    int metadataWarningsLogged = 0;
    for (const auto& warning : metadata.warnings()) {
        if (metadataWarningsLogged >= 5) {
            break;
        }
        erdup::logInfo("Metadata warning: " + warning);
        ++metadataWarningsLogged;
    }
    if (metadata.warnings().size() > static_cast<std::size_t>(metadataWarningsLogged)) {
        erdup::logInfo(
            "Metadata warnings truncated: remaining=" +
            std::to_string(metadata.warnings().size() - metadataWarningsLogged)
        );
    }

    const auto sellValueLoad = erdup::loadConfiguredSellValues(config);
    erdup::SellValueIndex sellValues;
    sellValues.add(sellValueLoad);
    erdup::logInfo(
        "Sell values loaded: entries=" + std::to_string(sellValues.size()) +
        ", configured=" + std::to_string(!config.sellValuePath.empty()) +
        ", warnings=" + std::to_string(sellValues.warnings().size())
    );
    int sellValueWarningsLogged = 0;
    for (const auto& warning : sellValues.warnings()) {
        if (sellValueWarningsLogged >= 5) {
            break;
        }
        erdup::logInfo("Sell value warning: " + warning);
        ++sellValueWarningsLogged;
    }
    if (sellValues.warnings().size() > static_cast<std::size_t>(sellValueWarningsLogged)) {
        erdup::logInfo(
            "Sell value warnings truncated: remaining=" +
            std::to_string(sellValues.warnings().size() - sellValueWarningsLogged)
        );
    }

    const auto guardReport = erdup::evaluateRuntimeGuards();
    erdup::logInfo("Runtime guard: " + erdup::describeRuntimeGuardReport(guardReport));
    if (!guardReport.allowed) {
        const auto diagnostics = erdup::buildRuntimeDiagnosticReport(guardReport, nullptr);
        erdup::logInfo("Runtime diagnostics: " + erdup::formatRuntimeDiagnosticReport(diagnostics));
        erdup::logError("Runtime guard blocked ERDuplicateSeller. The worker will remain inactive.");
        return;
    }

    const auto mutationRoutinePath = dllDirectory / "mutation_routines.ini";
    std::vector<erdup::MutationRoutineCandidate> mutationRoutineCandidates;
    std::vector<std::string> mutationRoutineWarnings;
    std::error_code mutationRoutineExistsError;
    const bool mutationRoutineFileExists = std::filesystem::exists(
        mutationRoutinePath,
        mutationRoutineExistsError
    );
    if (mutationRoutineExistsError) {
        mutationRoutineWarnings.push_back(
            "could not check mutation_routines.ini: " + mutationRoutineExistsError.message()
        );
    } else if (mutationRoutineFileExists) {
        const auto mutationRoutineLoad = erdup::loadMutationRoutineCandidates(mutationRoutinePath);
        mutationRoutineCandidates = mutationRoutineLoad.candidates;
        mutationRoutineWarnings = mutationRoutineLoad.warnings;
    }
    erdup::logInfo(
        "Mutation routine candidates: file_exists=" + std::to_string(mutationRoutineFileExists) +
        ", candidates=" + std::to_string(mutationRoutineCandidates.size()) +
        ", warnings=" + std::to_string(mutationRoutineWarnings.size())
    );
    int mutationRoutineWarningsLogged = 0;
    for (const auto& warning : mutationRoutineWarnings) {
        if (mutationRoutineWarningsLogged >= 5) {
            break;
        }
        erdup::logInfo("Mutation routine warning: " + warning);
        ++mutationRoutineWarningsLogged;
    }
    if (mutationRoutineWarnings.size() > static_cast<std::size_t>(mutationRoutineWarningsLogged)) {
        erdup::logInfo(
            "Mutation routine warnings truncated: remaining=" +
            std::to_string(mutationRoutineWarnings.size() - mutationRoutineWarningsLogged)
        );
    }

    const auto probeReport = erdup::runReadOnlyProbe(
        erdup::defaultReadOnlyProbeSignatures(),
        config.supportedGameVersion
    );
    erdup::logInfo("Read-only probe: " + erdup::describeReadOnlyProbeReport(probeReport));
    if (!erdup::runtimeVersionAllowsMemoryAccess(probeReport)) {
        erdup::logError(
            "Runtime blocked before memory access: observed game version=" +
            probeReport.module.fileVersion + ", validated version=" +
            probeReport.expectedGameVersion +
            ". Inventory reads, snapshots, commands and mutation remain disabled. "
            "Revalidate the inventory layout before changing supported_game_version."
        );
        return;
    }
    if (!mutationRoutineCandidates.empty()) {
        const auto* moduleBytes = probeReport.module.baseAddress == 0
            ? nullptr
            : reinterpret_cast<const std::uint8_t*>(probeReport.module.baseAddress);
        const auto resolutionReport = erdup::resolveMutationRoutineCandidatesForModule(
            mutationRoutineCandidates,
            probeReport.module,
            moduleBytes,
            probeReport.module.imageSize
        );
        erdup::logInfo(
            "Mutation routine resolution: " +
            erdup::formatMutationRoutineResolutionReport(resolutionReport)
        );
        for (const auto& resolution : resolutionReport.resolutions) {
            erdup::logInfo(erdup::formatMutationRoutineResolution(resolution));
        }
    }
    const auto mutationGate = erdup::evaluateMutationRoutineGate(
        mutationRoutineCandidates,
        config,
        probeReport.module.fileVersion
    );
    const auto diagnostics = erdup::buildRuntimeDiagnosticReport(guardReport, &probeReport, &mutationGate);
    erdup::logInfo("Runtime diagnostics: " + erdup::formatRuntimeDiagnosticReport(diagnostics));

    const auto chainsPath = dllDirectory / "pointer_chains.txt";

    erdup::StubGameAdapter adapter;
    if (config.nativeSaleEnabled) erdup::startSaleIndicator(config.hotkeyVirtualKey);
    erdup::MemoryScanSession memoryScanSession;

    // Resolve each saved candidate chain live and log the value it lands on, flagging any that
    // match the expected value. Triggered while in-game (not at load, when no save is active).
    const auto verifyChains = [&](std::uint64_t expectedValue) {
        std::ifstream chainsFile(chainsPath);
        if (!chainsFile) {
            erdup::logError("Chain verify: could not open " + chainsPath.string());
            return;
        }
        erdup::logInfo("Chain verify: resolving against expected value " + std::to_string(expectedValue));
        std::string line;
        int lineNumber = 0;
        int matches = 0;
        while (std::getline(chainsFile, line)) {
            ++lineNumber;
            const auto firstNonSpace = line.find_first_not_of(" \t\r\n");
            if (firstNonSpace == std::string::npos || line[firstNonSpace] == '#') {
                continue;
            }
            const auto chain = erdup::parsePointerChainSpec(line);
            if (!chain.has_value()) {
                erdup::logError("Chain verify: unparseable chain at line " + std::to_string(lineNumber));
                continue;
            }
            const auto verification = erdup::verifyPointerChainLive(
                probeReport.module.baseAddress,
                chain.value(),
                static_cast<std::size_t>(config.memoryScanValueSize)
            );
            const bool matched = verification.valueRead && verification.value == expectedValue;
            if (matched) {
                ++matches;
            }
            erdup::logInfo(
                std::string("Chain verify: ") + (matched ? "MATCH " : "      ") +
                erdup::formatPointerChainVerification(verification)
            );
        }
        erdup::logInfo("Chain verify: " + std::to_string(matches) + " chain(s) matched the expected value.");
    };

    const auto runFieldDump = [&]() {
        if (config.fieldDumpChain.empty()) {
            erdup::logError("Field dump blocked: field_dump_chain is not configured.");
            return;
        }

        const auto chain = erdup::parsePointerChainSpec(config.fieldDumpChain);
        if (!chain.has_value()) {
            erdup::logError("Field dump blocked: field_dump_chain is not parseable.");
            return;
        }

        erdup::logInfo(
            "Field dump start: bytes=" + std::to_string(config.fieldDumpBytes) +
            ", stride=" + std::to_string(config.fieldDumpStride)
        );
        const auto report = erdup::runFieldDumpLive(
            probeReport.module.baseAddress,
            chain.value(),
            static_cast<std::size_t>(config.fieldDumpBytes),
            static_cast<std::size_t>(config.fieldDumpStride)
        );
        erdup::logInfo("Field dump end: " + erdup::formatFieldDumpSummary(report));
        for (const auto& slot : report.slots) {
            erdup::logInfo(erdup::formatFieldDumpSlot(slot));
        }
    };

    const auto runInventoryRead = [&](bool includeDryRunReport) {
        if (config.inventoryBaseChain.empty()) {
            erdup::logError("Inventory read blocked: inventory_base_chain is not configured.");
            return;
        }

        const auto chain = erdup::parsePointerChainSpec(config.inventoryBaseChain);
        if (!chain.has_value()) {
            erdup::logError("Inventory read blocked: inventory_base_chain is not parseable.");
            return;
        }

        erdup::InventoryReadLayout layout;
        layout.baseChain = chain.value();
        layout.arrayOffset = static_cast<std::size_t>(config.inventoryArrayOffset);
        layout.countOffset = static_cast<std::size_t>(config.inventoryCountOffset);
        layout.capacityOffset = static_cast<std::size_t>(config.inventoryCapacityOffset);
        layout.entryStride = static_cast<std::size_t>(config.inventoryEntryStride);
        layout.handleOffset = static_cast<std::size_t>(config.inventoryHandleOffset);
        layout.itemIdOffset = static_cast<std::size_t>(config.inventoryItemIdOffset);
        layout.quantityOffset = static_cast<std::size_t>(config.inventoryQuantityOffset);
        layout.maxEntries = static_cast<std::size_t>(config.inventoryMaxEntries);
        layout.entryDiagnosticsEnabled = config.inventoryEntryDiagnosticsEnabled;
        layout.entryDiagnosticsMax = static_cast<std::size_t>(config.inventoryEntryDiagnosticsMax);
        layout.entryDiagnosticsIncludeConsumables = config.inventoryEntryDiagnosticsIncludeConsumables;
        layout.headerDumpOffset = static_cast<std::size_t>(config.inventorySnapshotHeaderOffset);
        layout.headerDumpBytes = config.inventorySnapshotExportEnabled
            ? static_cast<std::size_t>(config.inventorySnapshotHeaderBytes)
            : 0;
        layout.focusDumpStart = static_cast<std::size_t>(config.inventorySnapshotFocusStart);
        layout.focusDumpCount = config.inventorySnapshotExportEnabled
            ? static_cast<std::size_t>(config.inventorySnapshotFocusCount)
            : 0;

        const auto diagnosticName = [&](const erdup::InventoryEntryDiagnostic& diagnostic) {
            if (const auto* exact = metadata.find(diagnostic.category, diagnostic.baseItemId)) {
                return exact->name;
            }
            if (const auto* fallback = metadata.find(diagnostic.baseItemId)) {
                return fallback->name;
            }
            return std::string("<unknown>");
        };
        const auto handleDiagnosticName = [&](const erdup::HandleDiagnosticMatch& match) {
            if (const auto* exact = metadata.find(match.target.category, match.target.baseItemId)) {
                return exact->name;
            }
            if (const auto* fallback = metadata.find(match.target.baseItemId)) {
                return fallback->name;
            }
            return std::string("<unknown>");
        };
        const auto compactRowDiagnosticName = [&](const erdup::CompactRowDiagnosticMatch& match) {
            if (const auto* exact = metadata.find(match.target.category, match.target.baseItemId)) {
                return exact->name;
            }
            if (const auto* fallback = metadata.find(match.target.baseItemId)) {
                return fallback->name;
            }
            return std::string("<unknown>");
        };
        const auto compactRowDumpName = [&](const erdup::CompactRowDumpRow& row) {
            if (!row.hasTarget) {
                return std::string("<unknown>");
            }
            if (const auto* exact = metadata.find(row.target.category, row.target.baseItemId)) {
                return exact->name;
            }
            if (const auto* fallback = metadata.find(row.target.baseItemId)) {
                return fallback->name;
            }
            return std::string("<unknown>");
        };

        erdup::logInfo(
            "Inventory read start: max_entries=" + std::to_string(config.inventoryMaxEntries) +
            ", stride=" + std::to_string(config.inventoryEntryStride) +
            ", entry_diagnostics_enabled=" + std::to_string(config.inventoryEntryDiagnosticsEnabled) +
            ", entry_diagnostics_max=" + std::to_string(config.inventoryEntryDiagnosticsMax)
        );
        const auto readReport = erdup::readInventoryLive(probeReport.module.baseAddress, layout);
        erdup::logInfo("Inventory read end: " + erdup::formatInventoryReadSummary(readReport));
        if (config.inventorySnapshotExportEnabled && erdup::shouldExportInventorySnapshot(readReport)) {
            const auto snapshotPath = nextInventorySnapshotPath(dllDirectory);
            std::ofstream snapshotFile(snapshotPath, std::ios::binary);
            if (!snapshotFile) {
                erdup::logError("Inventory snapshot export failed: could not open " + snapshotPath.string());
            } else {
                snapshotFile << erdup::formatInventorySnapshotFixture(readReport);
                snapshotFile.close();
                erdup::logInfo("Inventory snapshot exported: path=\"" + snapshotPath.string() + "\"");
                if (config.inventorySnapshotRetentionCount > 0) {
                    const auto cleanupReport = erdup::cleanupInventorySnapshots(
                        dllDirectory,
                        static_cast<std::size_t>(config.inventorySnapshotRetentionCount)
                    );
                    erdup::logInfo(erdup::formatInventorySnapshotCleanupReport(cleanupReport));
                }
            }
        } else if (config.inventorySnapshotExportEnabled) {
            erdup::logInfo("Inventory snapshot export skipped: read did not produce exportable inventory rows.");
        }
        const auto skippedToLog = std::min(readReport.skipped.size(), kMaxLoggedInventoryReadSkips);
        for (std::size_t i = 0; i < skippedToLog; ++i) {
            erdup::logInfo(erdup::formatInventoryReadSkip(readReport.skipped[i]));
        }
        if (!readReport.skipped.empty()) {
            erdup::logInfo(erdup::formatInventoryReadSkipSummary(readReport.skipped, skippedToLog));
        }
        for (const auto& diagnostic : readReport.entryDiagnostics) {
            erdup::logInfo(
                erdup::formatInventoryEntryDiagnostic(diagnostic) +
                ", name=\"" + diagnosticName(diagnostic) + "\""
            );
        }

        if (config.inventoryCompactRowDiagnosticsEnabled) {
            erdup::CompactRowDiagnosticLimits compactLimits;
            compactLimits.maxTargets =
                static_cast<std::size_t>(config.inventoryCompactRowDiagnosticsMaxTargets);
            compactLimits.maxRows =
                static_cast<std::size_t>(config.inventoryCompactRowDiagnosticsMaxRows);
            compactLimits.maxBytesToScan =
                static_cast<std::size_t>(config.inventoryCompactRowDiagnosticsMaxBytesMb) * 1024ull * 1024ull;
            compactLimits.minRunLength =
                static_cast<std::size_t>(config.inventoryCompactRowDiagnosticsMinRunLength);
            compactLimits.maxRuns =
                static_cast<std::size_t>(config.inventoryCompactRowDiagnosticsMaxRuns);
            compactLimits.includeConsumables = config.inventoryCompactRowDiagnosticsIncludeConsumables;
            erdup::logInfo(
                "Compact row scan start: max_targets=" +
                std::to_string(config.inventoryCompactRowDiagnosticsMaxTargets) +
                ", max_rows=" + std::to_string(config.inventoryCompactRowDiagnosticsMaxRows) +
                ", min_run_length=" +
                std::to_string(config.inventoryCompactRowDiagnosticsMinRunLength) +
                ", max_bytes_mb=" + std::to_string(config.inventoryCompactRowDiagnosticsMaxBytesMb)
            );
            const auto compactReport = erdup::scanCompactRowsLive(
                probeReport.module.baseAddress,
                readReport.items,
                compactLimits
            );
            erdup::logInfo("Compact row scan end: " + erdup::formatCompactRowDiagnosticSummary(compactReport));
            for (const auto& run : compactReport.runs) {
                erdup::logInfo(erdup::formatCompactRowDiagnosticRun(run));
            }
            if (config.inventoryCompactRowDumpEnabled) {
                erdup::CompactRowDumpLimits dumpLimits;
                dumpLimits.maxRuns = static_cast<std::size_t>(config.inventoryCompactRowDumpMaxRuns);
                dumpLimits.maxRowsPerRun =
                    static_cast<std::size_t>(config.inventoryCompactRowDumpMaxRowsPerRun);
                dumpLimits.backtrackBytes =
                    static_cast<std::size_t>(config.inventoryCompactRowDumpBacktrackBytes);
                dumpLimits.maxStrideBytes =
                    static_cast<std::size_t>(config.inventoryCompactRowDumpMaxStrideBytes);
                const auto dumpReport = erdup::dumpCompactRowRunsLive(
                    probeReport.module.baseAddress,
                    compactReport,
                    dumpLimits
                );
                erdup::logInfo(erdup::formatCompactRowDumpSummary(dumpReport));
                for (const auto& run : dumpReport.runs) {
                    erdup::logInfo(erdup::formatCompactRowDumpRun(run));
                }
                for (const auto& row : dumpReport.rows) {
                    erdup::logInfo(
                        erdup::formatCompactRowDumpRow(row) +
                        ", name=\"" + compactRowDumpName(row) + "\""
                    );
                }
            }
            for (const auto& match : compactReport.matches) {
                erdup::logInfo(
                    erdup::formatCompactRowDiagnosticMatch(match) +
                    ", name=\"" + compactRowDiagnosticName(match) + "\""
                );
            }
        }

        if (config.inventoryHandleDiagnosticsEnabled) {
            erdup::HandleDiagnosticLimits handleLimits;
            handleLimits.maxHandles = static_cast<std::size_t>(config.inventoryHandleDiagnosticsMaxHandles);
            handleLimits.maxMatchesPerHandle =
                static_cast<std::size_t>(config.inventoryHandleDiagnosticsMaxMatchesPerHandle);
            handleLimits.maxBytesToScan =
                static_cast<std::size_t>(config.inventoryHandleDiagnosticsMaxBytesMb) * 1024ull * 1024ull;
            handleLimits.includeConsumables = config.inventoryHandleDiagnosticsIncludeConsumables;
            erdup::logInfo(
                "Handle reference scan start: max_handles=" +
                std::to_string(config.inventoryHandleDiagnosticsMaxHandles) +
                ", max_matches_per_handle=" +
                std::to_string(config.inventoryHandleDiagnosticsMaxMatchesPerHandle) +
                ", max_bytes_mb=" + std::to_string(config.inventoryHandleDiagnosticsMaxBytesMb)
            );
            const auto handleReport = erdup::scanHandleReferencesLive(
                probeReport.module.baseAddress,
                readReport.items,
                handleLimits
            );
            erdup::logInfo("Handle reference scan end: " + erdup::formatHandleDiagnosticSummary(handleReport));
            for (const auto& match : handleReport.matches) {
                erdup::logInfo(
                    erdup::formatHandleDiagnosticMatch(match) +
                    ", name=\"" + handleDiagnosticName(match) + "\""
                );
            }
        }

        if (includeDryRunReport) {
            const auto* metadataForReport = metadata.size() == 0 ? nullptr : &metadata;
            const auto* sellValuesForReport = config.sellValuePath.empty() ? nullptr : &sellValues;
            const auto salePlan = erdup::buildSnapshotSalePlan(
                readReport,
                config,
                erdup::CustomRemovalMode::MarkEmptyAndDecrementCount,
                metadataForReport,
                sellValuesForReport
            );
            erdup::logInfo("Inventory read sale preflight: " + erdup::formatSnapshotSalePlanSummary(salePlan));
            erdup::logInfo("Inventory read dry-run report:\n" + erdup::formatInventoryReport(salePlan.report));
            erdup::logInfo("Inventory read removal preflight: " + erdup::formatCustomRemovalPlan(salePlan.removalPlan));
            const auto mutationPreflight = erdup::evaluateMutationSafety(
                sellResultFromReportForMutationPreflight(salePlan.report),
                config
            );
            erdup::logInfo(
                "Inventory read mutation preflight: " +
                erdup::formatMutationSafetyReport(mutationPreflight)
            );
            const auto liveRoutineGate = erdup::evaluateMutationRoutineGate(
                mutationRoutineCandidates,
                config,
                probeReport.module.fileVersion
            );
            const auto liveMutationGate = erdup::evaluateLiveMutationGate(
                mutationPreflight,
                liveRoutineGate,
                salePlan.removalPlan
            );
            erdup::logInfo(
                "Inventory read live mutation gate: " +
                erdup::formatLiveMutationGateReport(liveMutationGate)
            );
        } else {
            erdup::logInfo("Inventory read dry-run report skipped for automatic snapshot.");
        }
    };

    // Shared scan steps, used by both the file-watch auto pipeline and the manual hotkeys.
    const auto runValueScan = [&](std::uint64_t targetValue) {
        erdup::MemoryScanLimits scanLimits;
        scanLimits.maxBytesToScan =
            static_cast<std::size_t>(config.memoryScanMaxBytesMb) * 1024u * 1024u;
        scanLimits.maxRegionsToVisit = static_cast<std::size_t>(config.memoryScanMaxRegions);
        scanLimits.maxCandidates = static_cast<std::size_t>(config.memoryScanMaxCandidates);
        erdup::logInfo(
            "Memory scan start: mode=" +
            std::string(memoryScanSession.hasPreviousCandidates() ? "narrow" : "initial") +
            ", target_value=" + std::to_string(targetValue)
        );
        const auto report = memoryScanSession.scanProcess(
            probeReport.module,
            targetValue,
            static_cast<std::size_t>(config.memoryScanValueSize),
            scanLimits
        );
        erdup::logInfo("Memory scan end: " + erdup::formatMemoryScanReport(report));
        return report;
    };

    const auto runPointerScanOnFirstCandidate = [&]() {
        const auto& candidates = memoryScanSession.currentCandidates();
        if (candidates.empty()) {
            erdup::logError("Pointer scan blocked: no value-scan candidates yet. Narrow the value scan first.");
            return;
        }
        // Resolve the first narrowed candidate back to a live absolute address.
        // Two's-complement addition handles candidates located below the module base.
        const auto targetAddress = probeReport.module.baseAddress +
            static_cast<std::uintptr_t>(candidates.front().moduleRelativeAddress);

        erdup::PointerScanLimits pointerLimits;
        pointerLimits.maxDepth = config.pointerScanMaxDepth;
        pointerLimits.offsetWindow = static_cast<std::size_t>(config.pointerScanOffsetWindow);
        pointerLimits.maxBytesToScan =
            static_cast<std::size_t>(config.memoryScanMaxBytesMb) * 1024u * 1024u;
        pointerLimits.maxRegionsToVisit = static_cast<std::size_t>(config.memoryScanMaxRegions);

        erdup::logInfo(
            "Pointer scan start: candidate 1 of " + std::to_string(candidates.size()) +
            ", max_depth=" + std::to_string(pointerLimits.maxDepth)
        );
        const auto report = erdup::scanProcessForPointerChains(
            probeReport.module, targetAddress, pointerLimits);
        erdup::logInfo("Pointer scan end: " + erdup::formatPointerScanReport(report));
    };

    bool wasPressed = false;
    bool scanWasPressed = false;
    bool pointerScanWasPressed = false;

    // Auto pipeline state: react to edits of memory_scan_target.txt instead of hotkeys.
    erdup::WatchedFileState lastTargetState;
    bool haveTargetState = false;
    std::string lastTargetWatchError;
    erdup::WatchedFileState lastRuntimeCommandState;
    bool haveRuntimeCommandState = false;
    std::string lastRuntimeCommandWatchError;
    std::uint64_t lastAutoValue = 0;
    bool haveAutoValue = false;
    std::intptr_t lastPointerRva = 0;
    bool havePointerScanned = false;
    erdup::InventorySnapshotScheduleState inventorySnapshotSchedule;

    {
        std::error_code existsError;
        const bool commandFileExists = std::filesystem::exists(runtimeCommandPath, existsError);
        if (existsError) {
            erdup::logError("Runtime command watch setup blocked: " + existsError.message());
        } else if (commandFileExists) {
            const auto state = erdup::readWatchedFileState(runtimeCommandPath);
            if (state.readable) {
                lastRuntimeCommandState = state;
                haveRuntimeCommandState = true;
                erdup::logInfo(
                    "Runtime command watch armed: path=\"" + runtimeCommandPath.string() +
                    "\" state=" + erdup::formatWatchedFileState(state)
                );
            } else {
                erdup::logError(
                    "Runtime command watch setup blocked: " +
                    erdup::formatWatchedFileState(state)
                );
            }
        } else {
            erdup::logInfo(
                "Runtime command watch armed: path=\"" + runtimeCommandPath.string() +
                "\". Write a command to this file to trigger a manual action."
            );
        }
    }

    const auto executeRuntimeCommand = [&](const erdup::RuntimeCommand& command) {
        if (!command.error.empty()) {
            erdup::logError("Runtime command blocked: " + erdup::formatRuntimeCommand(command));
            return;
        }

        switch (command.kind) {
        case erdup::RuntimeCommandKind::None:
            erdup::logInfo("Runtime command ignored: no command in file.");
            return;
        case erdup::RuntimeCommandKind::InventoryPreflight:
            config = erdup::loadConfig(configPath);
            if (!config.inventoryReadEnabled) {
                erdup::logError("Runtime command blocked: inventory_read_enabled is false.");
                return;
            }
            erdup::logInfo("Runtime command: running read-only live sale preflight.");
            runInventoryRead(true);
            return;
        case erdup::RuntimeCommandKind::InventorySnapshot:
            config = erdup::loadConfig(configPath);
            if (!config.inventoryReadEnabled) {
                erdup::logError("Runtime command blocked: inventory_read_enabled is false.");
                return;
            }
            erdup::logInfo("Runtime command: running read-only inventory snapshot.");
            runInventoryRead(false);
            return;
        case erdup::RuntimeCommandKind::SellDuplicates:
            config = erdup::loadConfig(configPath);
            erdup::runNativeSale(probeReport.module, config, metadata, sellValues, dllDirectory);
            return;
        case erdup::RuntimeCommandKind::FieldDump:
            config = erdup::loadConfig(configPath);
            erdup::logInfo("Runtime command: running field dump.");
            runFieldDump();
            return;
        case erdup::RuntimeCommandKind::VerifyChains:
            erdup::logInfo("Runtime command: verifying pointer chains.");
            verifyChains(command.value.value());
            return;
        case erdup::RuntimeCommandKind::Help:
            erdup::logInfo(erdup::runtimeCommandHelpText());
            return;
        case erdup::RuntimeCommandKind::Unknown:
            erdup::logError("Runtime command blocked: " + erdup::formatRuntimeCommand(command));
            return;
        }
    };

    const auto pollRuntimeCommand = [&]() {
        std::error_code existsError;
        const bool commandFileExists = std::filesystem::exists(runtimeCommandPath, existsError);
        if (existsError) {
            const auto message = "exists failed: " + existsError.message();
            if (message != lastRuntimeCommandWatchError) {
                erdup::logError("Runtime command watch blocked: " + message);
                lastRuntimeCommandWatchError = message;
            }
            return;
        }
        if (!commandFileExists) {
            haveRuntimeCommandState = false;
            lastRuntimeCommandWatchError.clear();
            return;
        }

        const auto currentCommandState = erdup::readWatchedFileState(runtimeCommandPath);
        const auto commandDecision = erdup::decideWatchedFileTrigger(
            haveRuntimeCommandState,
            lastRuntimeCommandState,
            currentCommandState
        );
        if (!currentCommandState.readable) {
            if (currentCommandState.error != lastRuntimeCommandWatchError) {
                erdup::logError(
                    "Runtime command watch blocked: " +
                    erdup::formatWatchedFileState(currentCommandState)
                );
                lastRuntimeCommandWatchError = currentCommandState.error;
            }
            return;
        }
        if (!commandDecision.changed) {
            return;
        }

        if (!lastRuntimeCommandWatchError.empty()) {
            erdup::logInfo(
                "Runtime command watch recovered: " +
                erdup::formatWatchedFileState(currentCommandState)
            );
            lastRuntimeCommandWatchError.clear();
        }

        lastRuntimeCommandState = currentCommandState;
        haveRuntimeCommandState = true;
        erdup::logInfo(
            "Runtime command trigger: reason=\"" + commandDecision.reason +
            "\" state=" + erdup::formatWatchedFileState(currentCommandState)
        );

        std::string readError;
        const auto commandText = readTextFile(runtimeCommandPath, readError);
        if (!readError.empty()) {
            erdup::logError("Runtime command blocked: " + readError);
            return;
        }

        const auto command = erdup::parseRuntimeCommandText(commandText);
        erdup::logInfo(erdup::formatRuntimeCommand(command));
        executeRuntimeCommand(command);
    };

    while (g_running.load()) {
        if (!config.enabled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        pollRuntimeCommand();

        DWORD foregroundPid = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
        // Also catch a completed tap between polls. Always consume the state so
        // a key pressed in another app cannot be replayed when focus returns.
        const auto hotkeyState = GetAsyncKeyState(config.hotkeyVirtualKey);
        const bool pressed = foregroundPid == GetCurrentProcessId() &&
            (hotkeyState & 0x8001) != 0;
        if (pressed && !wasPressed) {
            config = erdup::loadConfig(configPath);
            if (config.nativeSaleEnabled) {
                erdup::runNativeSale(probeReport.module, config, metadata, sellValues, dllDirectory);
                GetAsyncKeyState(config.hotkeyVirtualKey); // Discard taps made while the sale ran.
            } else if (config.inventoryReadEnabled) {
                erdup::logInfo("Duplicate seller hotkey: running read-only live sale preflight.");
                runInventoryRead(true);
            } else {
                const auto result = erdup::executeDuplicateSale(adapter, config);
                erdup::logInfo(result.message);
            }
        }
        wasPressed = pressed;

        const bool snapshotScheduleEnabled = erdup::inventorySnapshotScheduleEnabled(config);
        const bool fileWatchModeEnabled =
            config.autoScanEnabled ||
            config.pointerChainVerifyEnabled ||
            config.fieldDumpEnabled ||
            (config.inventoryReadEnabled && !snapshotScheduleEnabled);

        if (snapshotScheduleEnabled || fileWatchModeEnabled) {
            if (snapshotScheduleEnabled) {
                const auto scheduleAction = erdup::updateInventorySnapshotSchedule(
                    inventorySnapshotSchedule,
                    config,
                    std::chrono::steady_clock::now()
                );
                if (scheduleAction == erdup::InventorySnapshotScheduleAction::RunNow) {
                    config = erdup::loadConfig(configPath);
                    if (erdup::inventorySnapshotScheduleEnabled(config)) {
                        erdup::logInfo(
                            "Inventory snapshot auto trigger: interval_seconds=" +
                            std::to_string(config.inventorySnapshotAutoIntervalSeconds) +
                            ", retention_count=" +
                            std::to_string(config.inventorySnapshotRetentionCount)
                        );
                        runInventoryRead(config.inventorySnapshotAutoDryRunReportEnabled);
                        erdup::completeInventorySnapshotScheduleRun(
                            inventorySnapshotSchedule,
                            config,
                            std::chrono::steady_clock::now()
                        );
                    } else {
                        erdup::updateInventorySnapshotSchedule(
                            inventorySnapshotSchedule,
                            config,
                            std::chrono::steady_clock::now()
                        );
                    }
                }
            }

            // Hands-free pipeline driven by saving memory_scan_target.txt while in-game.
            // In verify mode, resolve the saved chains against the entered value. Otherwise,
            // scan/narrow and auto-run the pointer scan once the candidate set is small.
            // Field dump mode shares this trigger but only classifies qword slots under the
            // configured base chain; it does not require the file contents to be a target value.
            // Inventory read mode also shares this trigger and emits only a dry-run report.
            if (fileWatchModeEnabled) {
                const auto currentTargetState = erdup::readWatchedFileState(memoryScanTargetPath);
                const auto watchDecision = erdup::decideWatchedFileTrigger(
                    haveTargetState,
                    lastTargetState,
                    currentTargetState
                );
                if (!currentTargetState.readable) {
                    if (currentTargetState.error != lastTargetWatchError) {
                        erdup::logError("File watch blocked: " + erdup::formatWatchedFileState(currentTargetState));
                        lastTargetWatchError = currentTargetState.error;
                    }
                } else if (watchDecision.changed) {
                    if (!lastTargetWatchError.empty()) {
                        erdup::logInfo("File watch recovered: " + erdup::formatWatchedFileState(currentTargetState));
                        lastTargetWatchError.clear();
                    }
                    erdup::logInfo(
                        "File watch trigger: reason=\"" + watchDecision.reason +
                        "\" state=" + erdup::formatWatchedFileState(currentTargetState)
                    );
                    lastTargetState = currentTargetState;
                    haveTargetState = true;
                    config = erdup::loadConfig(configPath);

                    if (config.fieldDumpEnabled) {
                        runFieldDump();
                    }
                    if (config.inventoryReadEnabled) {
                        runInventoryRead(true);
                    }

                    if (config.autoScanEnabled || config.pointerChainVerifyEnabled) {
                        std::string targetError;
                        const auto target = erdup::loadMemoryScanTarget(
                            memoryScanTargetPath,
                            static_cast<std::size_t>(config.memoryScanValueSize),
                            targetError
                        );
                        if (!target.has_value()) {
                            erdup::logError("Auto scan blocked: " + targetError);
                        } else if (haveAutoValue && target.value() == lastAutoValue) {
                            erdup::logInfo(
                                "Target value unchanged (" + std::to_string(target.value()) +
                                "). Change the in-game value, then save a new number."
                            );
                        } else if (config.pointerChainVerifyEnabled) {
                            verifyChains(target.value());
                            lastAutoValue = target.value();
                            haveAutoValue = true;
                        } else {
                            const auto scanReport = runValueScan(target.value());
                            lastAutoValue = target.value();
                            haveAutoValue = true;

                            const auto count = scanReport.candidates.size();
                            const auto threshold = static_cast<std::size_t>(config.autoScanPointerThreshold);
                            if (count >= 1 && count <= threshold) {
                                const auto firstRva = scanReport.candidates.front().moduleRelativeAddress;
                                if (!havePointerScanned || firstRva != lastPointerRva) {
                                    havePointerScanned = true;
                                    lastPointerRva = firstRva;
                                    erdup::logInfo(
                                        "Auto scan: narrowed to " + std::to_string(count) +
                                        " candidate(s); running pointer scan automatically."
                                    );
                                    runPointerScanOnFirstCandidate();
                                } else {
                                    erdup::logInfo("Auto scan: candidate unchanged; pointer scan already run for it.");
                                }
                            } else if (count > threshold) {
                                erdup::logInfo(
                                    "Auto scan: " + std::to_string(count) + " candidates remain (> threshold " +
                                    std::to_string(config.autoScanPointerThreshold) +
                                    "). Change the value again and save to narrow further."
                                );
                            } else {
                                erdup::logInfo(
                                    "Auto scan: 0 candidates. The value may not have been captured; "
                                    "try a different, larger value."
                                );
                            }
                        }
                    }

                    const auto postTargetState = erdup::readWatchedFileState(memoryScanTargetPath);
                    const auto postDecision = erdup::decideWatchedFileTrigger(
                        true,
                        lastTargetState,
                        postTargetState
                    );
                    if (!postTargetState.readable) {
                        erdup::logError("File watch post-check blocked: " + erdup::formatWatchedFileState(postTargetState));
                    } else if (postDecision.changed) {
                        erdup::logInfo(
                            "File watch changed during processing; next poll will retrigger: reason=\"" +
                            postDecision.reason + "\" state=" + erdup::formatWatchedFileState(postTargetState)
                        );
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(config.nativeSaleEnabled ? 20 : 200));
            continue;
        }

        const bool scanConfigured =
            config.memoryScanEnabled &&
            config.memoryScanHotkeyVirtualKey > 0 &&
            config.memoryScanHotkeyVirtualKey != config.hotkeyVirtualKey;
        const bool scanPressed = scanConfigured &&
            ((GetAsyncKeyState(config.memoryScanHotkeyVirtualKey) & 0x8000) != 0);
        if (scanPressed && !scanWasPressed) {
            config = erdup::loadConfig(configPath);
            std::string targetError;
            const auto target = erdup::loadMemoryScanTarget(
                memoryScanTargetPath,
                static_cast<std::size_t>(config.memoryScanValueSize),
                targetError
            );
            if (!target.has_value()) {
                erdup::logError("Memory scan blocked: " + targetError);
            } else {
                runValueScan(target.value());
            }
        }
        scanWasPressed = scanPressed;

        const bool pointerScanConfigured =
            config.memoryScanEnabled &&
            config.pointerScanHotkeyVirtualKey > 0 &&
            config.pointerScanHotkeyVirtualKey != config.hotkeyVirtualKey &&
            config.pointerScanHotkeyVirtualKey != config.memoryScanHotkeyVirtualKey;
        const bool pointerScanPressed = pointerScanConfigured &&
            ((GetAsyncKeyState(config.pointerScanHotkeyVirtualKey) & 0x8000) != 0);
        if (pointerScanPressed && !pointerScanWasPressed) {
            config = erdup::loadConfig(configPath);
            runPointerScanOnFirstCandidate();
        }
        pointerScanWasPressed = pointerScanPressed;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

std::filesystem::path nextInventorySnapshotPath(const std::filesystem::path& directory) {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

    std::tm localTime{};
    localtime_s(&localTime, &time);

    std::ostringstream name;
    name << "inventory_snapshot_"
        << std::put_time(&localTime, "%Y%m%d_%H%M%S")
        << "_"
        << std::setw(3) << std::setfill('0') << millis
        << ".txt";
    return directory / name.str();
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        std::thread(workerThread).detach();
    } else if (reason == DLL_PROCESS_DETACH) {
        g_running.store(false);
    }

    return TRUE;
}
