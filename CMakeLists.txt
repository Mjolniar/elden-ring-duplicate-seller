cmake_minimum_required(VERSION 3.21)

project(ERDuplicateSeller VERSION 0.1.0 LANGUAGES CXX)

add_library(ERDuplicateSeller SHARED
    src/CompactRowDiagnostics.cpp
    src/Config.cpp
    src/CustomRemovalPlan.cpp
    src/DllMain.cpp
    src/DuplicateSeller.cpp
    src/FieldDump.cpp
    src/GameAdapterStub.cpp
    src/NativeSeller.cpp
    src/SaleIndicator.cpp
    src/HandleDiagnostics.cpp
    src/ItemMetadata.cpp
    src/InventoryChangeAnalysis.cpp
    src/InventoryReport.cpp
    src/InventoryReader.cpp
    src/InventorySnapshotRetention.cpp
    src/InventorySnapshotScheduler.cpp
    src/Logger.cpp
    src/LiveMutationGate.cpp
    src/MemoryReader.cpp
    src/MemoryScanner.cpp
    src/ModuleInfo.cpp
    src/MutationRoutineLoader.cpp
    src/MutationRoutineRegistry.cpp
    src/MutationRoutineResolver.cpp
    src/MutationSafety.cpp
    src/PatternScanner.cpp
    src/PointerScanner.cpp
    src/ReadOnlyProbe.cpp
    src/RuntimeCommand.cpp
    src/RuntimeGuards.cpp
    src/RuntimeDiagnostics.cpp
    src/SellValueMetadata.cpp
    src/SnapshotSalePlan.cpp
    src/WatchedFile.cpp
)

target_include_directories(ERDuplicateSeller PRIVATE include)
target_compile_features(ERDuplicateSeller PRIVATE cxx_std_20)
target_link_libraries(ERDuplicateSeller PRIVATE Version User32 Gdi32)

if(MSVC)
    target_compile_options(ERDuplicateSeller PRIVATE /W4 /permissive-)
    target_compile_definitions(ERDuplicateSeller PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(ERDuplicateSeller PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(ERDuplicateSeller PROPERTIES
    OUTPUT_NAME "ERDuplicateSeller"
)

add_executable(PolicySmokeTest
    src/DuplicateSeller.cpp
    src/MutationSafety.cpp
    tests/PolicySmokeTest.cpp
)

target_include_directories(PolicySmokeTest PRIVATE include)
target_compile_features(PolicySmokeTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(PolicySmokeTest PRIVATE /W4 /permissive-)
    target_compile_definitions(PolicySmokeTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(PolicySmokeTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME PolicySmokeTest COMMAND PolicySmokeTest)

add_executable(PolicyInvariantTest
    src/DuplicateSeller.cpp
    src/MutationSafety.cpp
    tests/PolicyInvariantTest.cpp
)

add_executable(MutationSafetyTest
    src/DuplicateSeller.cpp
    src/MutationSafety.cpp
    tests/MutationSafetyTest.cpp
)

target_include_directories(MutationSafetyTest PRIVATE include)
target_compile_features(MutationSafetyTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(MutationSafetyTest PRIVATE /W4 /permissive-)
    target_compile_definitions(MutationSafetyTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(MutationSafetyTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME MutationSafetyTest COMMAND MutationSafetyTest)

add_executable(LiveMutationGateTest
    src/LiveMutationGate.cpp
    src/MutationSafety.cpp
    tests/LiveMutationGateTest.cpp
)

target_include_directories(LiveMutationGateTest PRIVATE include)
target_compile_features(LiveMutationGateTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(LiveMutationGateTest PRIVATE /W4 /permissive-)
    target_compile_definitions(LiveMutationGateTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(LiveMutationGateTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME LiveMutationGateTest COMMAND LiveMutationGateTest)

add_executable(CustomRemovalPlanTest
    src/CustomRemovalPlan.cpp
    tests/CustomRemovalPlanTest.cpp
)

target_include_directories(CustomRemovalPlanTest PRIVATE include)
target_compile_features(CustomRemovalPlanTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(CustomRemovalPlanTest PRIVATE /W4 /permissive-)
    target_compile_definitions(CustomRemovalPlanTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(CustomRemovalPlanTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME CustomRemovalPlanTest COMMAND CustomRemovalPlanTest)

add_executable(InventoryChangeAnalysisTest
    src/CustomRemovalPlan.cpp
    src/InventoryChangeAnalysis.cpp
    tests/InventoryChangeAnalysisTest.cpp
)

target_include_directories(InventoryChangeAnalysisTest PRIVATE include)
target_compile_features(InventoryChangeAnalysisTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(InventoryChangeAnalysisTest PRIVATE /W4 /permissive-)
    target_compile_definitions(InventoryChangeAnalysisTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(InventoryChangeAnalysisTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME InventoryChangeAnalysisTest COMMAND InventoryChangeAnalysisTest)

add_executable(InventorySnapshotFixtureTest
    src/CustomRemovalPlan.cpp
    src/InventorySnapshotFixture.cpp
    tests/InventorySnapshotFixtureTest.cpp
)

target_include_directories(InventorySnapshotFixtureTest PRIVATE include)
target_compile_features(InventorySnapshotFixtureTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(InventorySnapshotFixtureTest PRIVATE /W4 /permissive-)
    target_compile_definitions(InventorySnapshotFixtureTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(InventorySnapshotFixtureTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME InventorySnapshotFixtureTest COMMAND InventorySnapshotFixtureTest)

add_executable(SnapshotSalePlanTest
    src/Config.cpp
    src/CustomRemovalPlan.cpp
    src/DuplicateSeller.cpp
    src/InventoryReport.cpp
    src/InventorySnapshotFixture.cpp
    src/ItemMetadata.cpp
    src/MutationSafety.cpp
    src/SellValueMetadata.cpp
    src/SnapshotSalePlan.cpp
    tests/SnapshotSalePlanTest.cpp
)

target_include_directories(SnapshotSalePlanTest PRIVATE include)
target_compile_features(SnapshotSalePlanTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(SnapshotSalePlanTest PRIVATE /W4 /permissive-)
    target_compile_definitions(SnapshotSalePlanTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(SnapshotSalePlanTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME SnapshotSalePlanTest COMMAND SnapshotSalePlanTest)

add_executable(MutationRoutineRegistryTest
    src/MutationRoutineRegistry.cpp
    tests/MutationRoutineRegistryTest.cpp
)

target_include_directories(MutationRoutineRegistryTest PRIVATE include)
target_compile_features(MutationRoutineRegistryTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(MutationRoutineRegistryTest PRIVATE /W4 /permissive-)
    target_compile_definitions(MutationRoutineRegistryTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(MutationRoutineRegistryTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME MutationRoutineRegistryTest COMMAND MutationRoutineRegistryTest)

add_executable(MutationRoutineLoaderTest
    src/MutationRoutineLoader.cpp
    src/MutationRoutineRegistry.cpp
    tests/MutationRoutineLoaderTest.cpp
)

target_include_directories(MutationRoutineLoaderTest PRIVATE include)
target_compile_features(MutationRoutineLoaderTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(MutationRoutineLoaderTest PRIVATE /W4 /permissive-)
    target_compile_definitions(MutationRoutineLoaderTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(MutationRoutineLoaderTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME MutationRoutineLoaderTest COMMAND MutationRoutineLoaderTest)

add_executable(MutationRoutineResolverTest
    src/MutationRoutineRegistry.cpp
    src/MutationRoutineResolver.cpp
    src/PatternScanner.cpp
    tests/MutationRoutineResolverTest.cpp
)

target_include_directories(MutationRoutineResolverTest PRIVATE include)
target_compile_features(MutationRoutineResolverTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(MutationRoutineResolverTest PRIVATE /W4 /permissive-)
    target_compile_definitions(MutationRoutineResolverTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(MutationRoutineResolverTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME MutationRoutineResolverTest COMMAND MutationRoutineResolverTest)

target_include_directories(PolicyInvariantTest PRIVATE include)
target_compile_features(PolicyInvariantTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(PolicyInvariantTest PRIVATE /W4 /permissive-)
    target_compile_definitions(PolicyInvariantTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(PolicyInvariantTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME PolicyInvariantTest COMMAND PolicyInvariantTest)

add_executable(ConfigTest
    src/Config.cpp
    tests/ConfigTest.cpp
)

target_include_directories(ConfigTest PRIVATE include)
target_compile_features(ConfigTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(ConfigTest PRIVATE /W4 /permissive-)
    target_compile_definitions(ConfigTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(ConfigTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME ConfigTest COMMAND ConfigTest)

add_executable(ConfigFileTest
    src/ConfigFile.cpp
    tests/ConfigFileTest.cpp
)

target_include_directories(ConfigFileTest PRIVATE include)
target_compile_features(ConfigFileTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(ConfigFileTest PRIVATE /W4 /permissive-)
    target_compile_definitions(ConfigFileTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(ConfigFileTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME ConfigFileTest COMMAND ConfigFileTest)

add_executable(WatchedFileTest
    src/WatchedFile.cpp
    tests/WatchedFileTest.cpp
)

target_include_directories(WatchedFileTest PRIVATE include)
target_compile_features(WatchedFileTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(WatchedFileTest PRIVATE /W4 /permissive-)
    target_compile_definitions(WatchedFileTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(WatchedFileTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME WatchedFileTest COMMAND WatchedFileTest)

add_executable(RuntimeCommandTest
    src/RuntimeCommand.cpp
    tests/RuntimeCommandTest.cpp
)

target_include_directories(RuntimeCommandTest PRIVATE include)
target_compile_features(RuntimeCommandTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(RuntimeCommandTest PRIVATE /W4 /permissive-)
    target_compile_definitions(RuntimeCommandTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(RuntimeCommandTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME RuntimeCommandTest COMMAND RuntimeCommandTest)

add_executable(InventorySnapshotSchedulerTest
    src/InventorySnapshotScheduler.cpp
    tests/InventorySnapshotSchedulerTest.cpp
)

target_include_directories(InventorySnapshotSchedulerTest PRIVATE include)
target_compile_features(InventorySnapshotSchedulerTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(InventorySnapshotSchedulerTest PRIVATE /W4 /permissive-)
    target_compile_definitions(InventorySnapshotSchedulerTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(InventorySnapshotSchedulerTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME InventorySnapshotSchedulerTest COMMAND InventorySnapshotSchedulerTest)

add_executable(InventorySnapshotRetentionTest
    src/InventorySnapshotRetention.cpp
    tests/InventorySnapshotRetentionTest.cpp
)

target_include_directories(InventorySnapshotRetentionTest PRIVATE include)
target_compile_features(InventorySnapshotRetentionTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(InventorySnapshotRetentionTest PRIVATE /W4 /permissive-)
    target_compile_definitions(InventorySnapshotRetentionTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(InventorySnapshotRetentionTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME InventorySnapshotRetentionTest COMMAND InventorySnapshotRetentionTest)

add_executable(ConfigEditor
    src/ConfigEditor.cpp
    src/ConfigFile.cpp
)

target_include_directories(ConfigEditor PRIVATE include)
target_compile_features(ConfigEditor PRIVATE cxx_std_20)
set_target_properties(ConfigEditor PROPERTIES OUTPUT_NAME "ERDuplicateSellerConfigEditor")

if(MSVC)
    target_compile_options(ConfigEditor PRIVATE /W4 /permissive-)
    target_compile_definitions(ConfigEditor PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(ConfigEditor PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(PatternScannerTest
    src/PatternScanner.cpp
    tests/PatternScannerTest.cpp
)

target_include_directories(PatternScannerTest PRIVATE include)
target_compile_features(PatternScannerTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(PatternScannerTest PRIVATE /W4 /permissive-)
    target_compile_definitions(PatternScannerTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(PatternScannerTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME PatternScannerTest COMMAND PatternScannerTest)

add_executable(MemoryReaderTest
    src/MemoryReader.cpp
    tests/MemoryReaderTest.cpp
)

target_include_directories(MemoryReaderTest PRIVATE include)
target_compile_features(MemoryReaderTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(MemoryReaderTest PRIVATE /W4 /permissive-)
    target_compile_definitions(MemoryReaderTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(MemoryReaderTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME MemoryReaderTest COMMAND MemoryReaderTest)

add_executable(MemoryScannerTest
    src/MemoryReader.cpp
    src/MemoryScanner.cpp
    tests/MemoryScannerTest.cpp
)

target_include_directories(MemoryScannerTest PRIVATE include)
target_compile_features(MemoryScannerTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(MemoryScannerTest PRIVATE /W4 /permissive-)
    target_compile_definitions(MemoryScannerTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(MemoryScannerTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME MemoryScannerTest COMMAND MemoryScannerTest)

add_executable(PointerScannerTest
    src/MemoryReader.cpp
    src/PointerScanner.cpp
    tests/PointerScannerTest.cpp
)

target_include_directories(PointerScannerTest PRIVATE include)
target_compile_features(PointerScannerTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(PointerScannerTest PRIVATE /W4 /permissive-)
    target_compile_definitions(PointerScannerTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(PointerScannerTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME PointerScannerTest COMMAND PointerScannerTest)

add_executable(FieldDumpTest
    src/FieldDump.cpp
    src/MemoryReader.cpp
    src/PointerScanner.cpp
    tests/FieldDumpTest.cpp
)

target_include_directories(FieldDumpTest PRIVATE include)
target_compile_features(FieldDumpTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(FieldDumpTest PRIVATE /W4 /permissive-)
    target_compile_definitions(FieldDumpTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(FieldDumpTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME FieldDumpTest COMMAND FieldDumpTest)

add_executable(CompactRowDiagnosticsTest
    src/CompactRowDiagnostics.cpp
    src/MemoryReader.cpp
    tests/CompactRowDiagnosticsTest.cpp
)

target_include_directories(CompactRowDiagnosticsTest PRIVATE include)
target_compile_features(CompactRowDiagnosticsTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(CompactRowDiagnosticsTest PRIVATE /W4 /permissive-)
    target_compile_definitions(CompactRowDiagnosticsTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(CompactRowDiagnosticsTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME CompactRowDiagnosticsTest COMMAND CompactRowDiagnosticsTest)

add_executable(HandleDiagnosticsTest
    src/HandleDiagnostics.cpp
    src/MemoryReader.cpp
    tests/HandleDiagnosticsTest.cpp
)

target_include_directories(HandleDiagnosticsTest PRIVATE include)
target_compile_features(HandleDiagnosticsTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(HandleDiagnosticsTest PRIVATE /W4 /permissive-)
    target_compile_definitions(HandleDiagnosticsTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(HandleDiagnosticsTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME HandleDiagnosticsTest COMMAND HandleDiagnosticsTest)

add_executable(InventoryReaderTest
    src/Config.cpp
    src/CustomRemovalPlan.cpp
    src/DuplicateSeller.cpp
    src/InventoryReader.cpp
    src/InventoryReport.cpp
    src/InventorySnapshotFixture.cpp
    src/ItemMetadata.cpp
    src/MemoryReader.cpp
    src/MutationSafety.cpp
    src/PointerScanner.cpp
    src/SellValueMetadata.cpp
    tests/InventoryReaderTest.cpp
)

target_include_directories(InventoryReaderTest PRIVATE include)
target_compile_features(InventoryReaderTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(InventoryReaderTest PRIVATE /W4 /permissive-)
    target_compile_definitions(InventoryReaderTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(InventoryReaderTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME InventoryReaderTest COMMAND InventoryReaderTest)

add_executable(ItemMetadataTest
    src/ItemMetadata.cpp
    tests/ItemMetadataTest.cpp
)

target_include_directories(ItemMetadataTest PRIVATE include)
target_compile_features(ItemMetadataTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(ItemMetadataTest PRIVATE /W4 /permissive-)
    target_compile_definitions(ItemMetadataTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(ItemMetadataTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME ItemMetadataTest COMMAND ItemMetadataTest)

add_executable(SellValueMetadataTest
    src/SellValueMetadata.cpp
    tests/SellValueMetadataTest.cpp
)

target_include_directories(SellValueMetadataTest PRIVATE include)
target_compile_features(SellValueMetadataTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(SellValueMetadataTest PRIVATE /W4 /permissive-)
    target_compile_definitions(SellValueMetadataTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(SellValueMetadataTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME SellValueMetadataTest COMMAND SellValueMetadataTest)

add_executable(InventoryReportTest
    src/DuplicateSeller.cpp
    src/InventoryReport.cpp
    src/ItemMetadata.cpp
    src/MutationSafety.cpp
    src/SellValueMetadata.cpp
    tests/InventoryReportTest.cpp
)

target_include_directories(InventoryReportTest PRIVATE include)
target_compile_features(InventoryReportTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(InventoryReportTest PRIVATE /W4 /permissive-)
    target_compile_definitions(InventoryReportTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(InventoryReportTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME InventoryReportTest COMMAND InventoryReportTest)

add_executable(MetadataIndexTest
    src/Config.cpp
    src/ItemMetadata.cpp
    tests/MetadataIndexTest.cpp
)

target_include_directories(MetadataIndexTest PRIVATE include)
target_compile_features(MetadataIndexTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(MetadataIndexTest PRIVATE /W4 /permissive-)
    target_compile_definitions(MetadataIndexTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(MetadataIndexTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME MetadataIndexTest COMMAND MetadataIndexTest)

add_executable(InventoryFixtureTest
    src/InventoryFixture.cpp
    src/ItemMetadata.cpp
    tests/InventoryFixtureTest.cpp
)

target_include_directories(InventoryFixtureTest PRIVATE include)
target_compile_features(InventoryFixtureTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(InventoryFixtureTest PRIVATE /W4 /permissive-)
    target_compile_definitions(InventoryFixtureTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(InventoryFixtureTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME InventoryFixtureTest COMMAND InventoryFixtureTest)

add_executable(DefaultPolicyContractTest
    src/DuplicateSeller.cpp
    src/InventoryFixture.cpp
    src/InventoryReport.cpp
    src/ItemMetadata.cpp
    src/MutationSafety.cpp
    src/SellValueMetadata.cpp
    tests/DefaultPolicyContractTest.cpp
)

target_include_directories(DefaultPolicyContractTest PRIVATE include)
target_compile_features(DefaultPolicyContractTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(DefaultPolicyContractTest PRIVATE /W4 /permissive-)
    target_compile_definitions(DefaultPolicyContractTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(DefaultPolicyContractTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME DefaultPolicyContractTest COMMAND DefaultPolicyContractTest)

add_executable(RuntimeDiagnosticsTest
    src/MutationRoutineRegistry.cpp
    src/RuntimeDiagnostics.cpp
    tests/RuntimeDiagnosticsTest.cpp
)

target_include_directories(RuntimeDiagnosticsTest PRIVATE include)
target_compile_features(RuntimeDiagnosticsTest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(RuntimeDiagnosticsTest PRIVATE /W4 /permissive-)
    target_compile_definitions(RuntimeDiagnosticsTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(RuntimeDiagnosticsTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME RuntimeDiagnosticsTest COMMAND RuntimeDiagnosticsTest)

add_executable(ReadOnlyProbeTest
    src/ModuleInfo.cpp
    src/PatternScanner.cpp
    src/ReadOnlyProbe.cpp
    tests/ReadOnlyProbeTest.cpp
)

target_include_directories(ReadOnlyProbeTest PRIVATE include)
target_compile_features(ReadOnlyProbeTest PRIVATE cxx_std_20)
target_link_libraries(ReadOnlyProbeTest PRIVATE Version)

if(MSVC)
    target_compile_options(ReadOnlyProbeTest PRIVATE /W4 /permissive-)
    target_compile_definitions(ReadOnlyProbeTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(ReadOnlyProbeTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME ReadOnlyProbeTest COMMAND ReadOnlyProbeTest)

find_program(POWERSHELL_EXE powershell)
find_program(PYTHON_EXE python)
if(PYTHON_EXE)
    add_test(
        NAME SellValueExtractorSelfTest
        COMMAND ${PYTHON_EXE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_sell_values.py
            --self-test
    )
endif()

if(POWERSHELL_EXE)
    add_test(
        NAME ProbeLogCheckerTest
        COMMAND ${POWERSHELL_EXE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_probe_log.ps1
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/probe_log_sample.txt
    )
endif()

add_executable(InventoryReportCli
    src/Config.cpp
    src/DuplicateSeller.cpp
    src/InventoryFixture.cpp
    src/InventoryReport.cpp
    src/InventoryReportCli.cpp
    src/ItemMetadata.cpp
    src/MutationSafety.cpp
    src/SellValueMetadata.cpp
)

target_include_directories(InventoryReportCli PRIVATE include)
target_compile_features(InventoryReportCli PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(InventoryReportCli PRIVATE /W4 /permissive-)
    target_compile_definitions(InventoryReportCli PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(InventoryReportCli PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(InventoryChangeCli
    src/CustomRemovalPlan.cpp
    src/InventoryChangeAnalysis.cpp
    src/InventoryChangeCli.cpp
    src/InventorySnapshotFixture.cpp
)

target_include_directories(InventoryChangeCli PRIVATE include)
target_compile_features(InventoryChangeCli PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(InventoryChangeCli PRIVATE /W4 /permissive-)
    target_compile_definitions(InventoryChangeCli PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(InventoryChangeCli PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(SnapshotSalePlanCli
    src/Config.cpp
    src/CustomRemovalPlan.cpp
    src/DuplicateSeller.cpp
    src/InventoryReport.cpp
    src/InventorySnapshotFixture.cpp
    src/ItemMetadata.cpp
    src/MutationSafety.cpp
    src/SellValueMetadata.cpp
    src/SnapshotSalePlan.cpp
    src/SnapshotSalePlanCli.cpp
)

target_include_directories(SnapshotSalePlanCli PRIVATE include)
target_compile_features(SnapshotSalePlanCli PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(SnapshotSalePlanCli PRIVATE /W4 /permissive-)
    target_compile_definitions(SnapshotSalePlanCli PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(SnapshotSalePlanCli PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(POWERSHELL_EXE)
    add_test(
        NAME InventoryChangeCliFixtureTest
        COMMAND ${POWERSHELL_EXE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_inventory_change_cli_tests.ps1
            $<TARGET_FILE:InventoryChangeCli>
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

if(POWERSHELL_EXE)
    add_test(
        NAME InventoryReportCliFixtureTest
        COMMAND ${POWERSHELL_EXE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_inventory_report_cli_tests.ps1
            $<TARGET_FILE:InventoryReportCli>
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
