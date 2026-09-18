#include "MutationSafety.h"

#include <algorithm>
#include <sstream>

namespace erdup {

namespace {

void addBlocker(std::vector<MutationBlocker>& blockers, MutationBlocker blocker) {
    if (std::find(blockers.begin(), blockers.end(), blocker) == blockers.end()) {
        blockers.push_back(blocker);
    }
}

} // namespace

MutationSafetyReport evaluateMutationSafety(const SellResult& plan, const Config& config) {
    MutationSafetyReport report;

    if (config.dryRun) {
        addBlocker(report.blockers, MutationBlocker::DryRunEnabled);
    }
    if (plan.candidates.empty() || plan.totalItems <= 0) {
        addBlocker(report.blockers, MutationBlocker::NoCandidates);
    }

    for (const auto& candidate : plan.candidates) {
        if (candidate.quantityToSell <= 0) {
            addBlocker(report.blockers, MutationBlocker::NonPositiveQuantity);
        }
        if (candidate.item.sellValue <= 0) {
            addBlocker(report.blockers, MutationBlocker::UnknownSellValue);
        }
        if (candidate.runeValue <= 0) {
            addBlocker(report.blockers, MutationBlocker::NonPositiveRuneTotal);
        }
    }

    if (plan.totalRunes <= 0 && !plan.candidates.empty()) {
        addBlocker(report.blockers, MutationBlocker::NonPositiveRuneTotal);
    }

    report.allowed = report.blockers.empty();
    return report;
}

std::string mutationBlockerReason(MutationBlocker blocker) {
    switch (blocker) {
    case MutationBlocker::DryRunEnabled:
        return "dry_run is enabled";
    case MutationBlocker::NoCandidates:
        return "no sale candidates";
    case MutationBlocker::UnknownSellValue:
        return "one or more sale candidates have unknown or non-positive sell value";
    case MutationBlocker::NonPositiveQuantity:
        return "one or more sale candidates have non-positive quantity";
    case MutationBlocker::NonPositiveRuneTotal:
        return "planned rune total is non-positive";
    }
    return "unknown mutation blocker";
}

std::string formatMutationSafetyReport(const MutationSafetyReport& report) {
    if (report.allowed) {
        return "mutation preflight passed";
    }

    std::ostringstream text;
    text << "mutation preflight blocked";
    for (std::size_t i = 0; i < report.blockers.size(); ++i) {
        text << (i == 0 ? ": " : "; ") << mutationBlockerReason(report.blockers[i]);
    }
    return text.str();
}

} // namespace erdup
