#include "GameAdapter.h"
#include "Logger.h"

namespace erdup {

bool StubGameAdapter::isReady() const {
    return false;
}

std::vector<ItemInstance> StubGameAdapter::readInventory() {
    return {};
}

bool StubGameAdapter::removeItem(const ItemInstance&, int) {
    logError("removeItem called on the stub adapter.");
    return false;
}

bool StubGameAdapter::addRunes(int) {
    logError("addRunes called on the stub adapter.");
    return false;
}

void StubGameAdapter::showToast(const std::string& message) {
    logInfo("Toast placeholder: " + message);
}

} // namespace erdup
