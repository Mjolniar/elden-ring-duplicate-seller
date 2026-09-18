#pragma once
#include "Config.h"
#include "ItemMetadata.h"
#include "ModuleInfo.h"
#include "SellValueMetadata.h"
#include <filesystem>
#include <string>

namespace erdup {
// Provisional native adapter for the independently identified 2.7.1.0 routines.
// Every sale is followed by inventory and rune-delta verification.
std::string runNativeSale(const ModuleInfo&, const Config&, const MetadataIndex&,
                          const SellValueIndex&, const std::filesystem::path&);
void startSaleIndicator(int virtualKey);
void setSaleIndicator(const std::string& text, bool running = false);
}
