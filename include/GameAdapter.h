#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace erdup {

enum class ItemCategory {
    Weapon,
    Shield,
    Armor,
    Talisman,
    AshOfWar,
    Spell,
    Consumable,
    Material,
    KeyItem,
    Unknown,
};

struct ItemInstance {
    std::uint32_t instanceId = 0;
    std::uint32_t baseItemId = 0;
    ItemCategory category = ItemCategory::Unknown;
    int quantity = 1;
    int upgradeLevel = 0;
    bool modified = false;
    int sellValue = 0;
    std::string debugName;
    bool equipped = false;
};

class IGameAdapter {
public:
    virtual ~IGameAdapter() = default;

    virtual bool isReady() const = 0;
    virtual std::vector<ItemInstance> readInventory() = 0;
    virtual bool removeItem(const ItemInstance& item, int quantity) = 0;
    virtual bool addRunes(int amount) = 0;
    virtual void showToast(const std::string& message) = 0;
};

class StubGameAdapter final : public IGameAdapter {
public:
    bool isReady() const override;
    std::vector<ItemInstance> readInventory() override;
    bool removeItem(const ItemInstance& item, int quantity) override;
    bool addRunes(int amount) override;
    void showToast(const std::string& message) override;
};

} // namespace erdup
