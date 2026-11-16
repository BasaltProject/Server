#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/world/EquipmentTables.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <array>
#include <expected>
#include <optional>
#include <string_view>

namespace basalt::server::runtime {

using player::PlayerSession;

namespace {

using entity::mobEquipment::Slot;
using player::PlayerInventory;
using server::world::loot::ItemStack;
using wire::protocol::play::Gamemode;

/// parity: `EquipmentSlot.getFilterBit`, the bit a slot owns in `disabledSlots`.
[[nodiscard]] std::int32_t filterBit(Slot slot) {
    switch (slot) {
    case Slot::MainHand:
        return 0;
    case Slot::OffHand:
        return 5;
    case Slot::Feet:
        return 1;
    case Slot::Legs:
        return 2;
    case Slot::Chest:
        return 3;
    case Slot::Head:
        return 4;
    case Slot::Body:
        return 6;
    case Slot::Saddle:
        return 7;
    }
    return 0;
}

[[nodiscard]] std::string_view slotName(Slot slot) {
    switch (slot) {
    case Slot::MainHand:
        return "main_hand";
    case Slot::OffHand:
        return "off_hand";
    default:
        return entity::mobEquipment::keyOf(slot);
    }
}

[[nodiscard]] bool isHand(Slot slot) { return slot == Slot::MainHand || slot == Slot::OffHand; }

class Stand {
public:
    explicit Stand(entity::ServerEntity& entity) : entity_(entity) {
        if (entity.sourceNbt.has_value()) {
            disabled_ = entity.sourceNbt->getInt("DisabledSlots");
            showArms_ = entity.sourceNbt->getBoolean("ShowArms");
            small_ = entity.sourceNbt->getBoolean("Small");
            marker_ = entity.sourceNbt->getBoolean("Marker");
        }
    }

    [[nodiscard]] bool marker() const { return marker_; }

    [[nodiscard]] bool showArms() const { return showArms_; }

    [[nodiscard]] bool small() const { return small_; }

    [[nodiscard]] std::optional<ItemStack> item(Slot slot) const { return entity::mobEquipment::get(entity_, slot); }

    [[nodiscard]] bool has(Slot slot) const { return item(slot).has_value(); }

    [[nodiscard]] bool bit(Slot slot, std::int32_t offset) const {
        return ((disabled_ >> (filterBit(slot) + offset)) & 1) != 0;
    }

    [[nodiscard]] bool isDisabled(Slot slot) const { return bit(slot, 0) || (isHand(slot) && !showArms_); }

    /// parity: `ArmorStand.canUseSlot`.
    [[nodiscard]] bool canUse(Slot slot) const {
        return slot != Slot::Body && slot != Slot::Saddle && !isDisabled(slot);
    }

    /// parity: `LivingEntity.getEquipmentSlotForItem`.
    [[nodiscard]] Slot slotForItem(const std::optional<ItemStack>& held) const {
        if (!held.has_value()) {
            return Slot::MainHand;
        }
        const std::optional<std::string_view> worn = world::equipmentTables::equippableSlot(*held);
        if (!worn.has_value()) {
            return Slot::MainHand;
        }
        static constexpr std::array<std::pair<std::string_view, Slot>, 6> kSlots{{{"mainhand", Slot::MainHand},
                                                                                  {"offhand", Slot::OffHand},
                                                                                  {"feet", Slot::Feet},
                                                                                  {"legs", Slot::Legs},
                                                                                  {"chest", Slot::Chest},
                                                                                  {"head", Slot::Head}}};
        for (const auto& [name, slot] : kSlots) {
            if (name == *worn) {
                return canUse(slot) ? slot : Slot::MainHand;
            }
        }
        return Slot::MainHand;
    }

    /// parity: `ArmorStand.getClickedSlot`.
    [[nodiscard]] Slot clickedSlot(double y) const {
        if (y >= 0.1 && y < 0.1 + (small_ ? 0.8 : 0.45) && has(Slot::Feet)) {
            return Slot::Feet;
        }
        if (y >= 0.9 + (small_ ? 0.3 : 0.0) && y < 0.9 + (small_ ? 1.0 : 0.7) && has(Slot::Chest)) {
            return Slot::Chest;
        }
        if (y >= 0.4 && y < 0.4 + (small_ ? 1.0 : 0.8) && has(Slot::Legs)) {
            return Slot::Legs;
        }
        if (y >= 1.6 && has(Slot::Head)) {
            return Slot::Head;
        }
        if (!has(Slot::MainHand) && has(Slot::OffHand)) {
            return Slot::OffHand;
        }
        return Slot::MainHand;
    }

    void set(Slot slot, const std::optional<ItemStack>& stack) {
        entity::mobEquipment::set(entity_, slot, stack, false);
    }

    [[nodiscard]] entity::ServerEntity& entity() const { return entity_; }

private:
    entity::ServerEntity& entity_;
    std::int32_t disabled_ = 0;
    bool showArms_ = false;
    bool small_ = false;
    bool marker_ = false;
};

} // namespace

CombatHandler::ArmorStandUse CombatHandler::useArmorStand(PlayerSession& session, entity::ServerEntity& armorStand,
                                                          bool mainHand, double clickY) {
    Stand stand{armorStand};
    const std::optional<ItemStack> inHand = [&]() -> std::optional<ItemStack> {
        const std::int32_t index =
            mainHand ? PlayerInventory::kHotbarStart + session.inventory.heldSlot() : PlayerInventory::kOffhandSlot;
        const std::optional<storage::StoredItemStack> stored = session.inventory.at(index);
        if (!stored.has_value() || stored->count <= 0) {
            return std::nullopt;
        }
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            wire::common::Identifier::parse(stored->id);
        return id.has_value()
                   ? std::optional{ItemStack{.item = *id, .count = stored->count, .components = stored->components}}
                   : std::nullopt;
    }();
    // parity: `ArmorStand.interact`, a marker or a name tag goes on to `Entity.interact`.
    if (stand.marker() || (inHand.has_value() && inHand->item.path() == "name_tag")) {
        return ArmorStandUse::Pass;
    }
    const std::int32_t handIndex =
        mainHand ? PlayerInventory::kHotbarStart + session.inventory.heldSlot() : PlayerInventory::kOffhandSlot;
    const Slot itemSlot = stand.slotForItem(inHand);
    const auto swap = [&](Slot slot) {
        const std::optional<ItemStack> onStand = stand.item(slot);
        // parity: `ArmorStand.swapItem`, a locked slot to remove from or to fill.
        if ((onStand.has_value() && stand.bit(slot, 8)) || (!onStand.has_value() && stand.bit(slot, 16))) {
            return false;
        }
        if (session.events != nullptr &&
            !session.events->allowed(
                &server::plugins::pluginEventTypes::playerArmorStandManipulate,
                [&](server::plugins::PluginEvent& event) {
                    plugins::EventSink::player(event, session);
                    event.set("entity_id", static_cast<std::int64_t>(armorStand.entityId()));
                    event.set("slot", std::string{slotName(slot)});
                    event.set("hand", std::string{mainHand ? "main_hand" : "off_hand"});
                    event.set("item", inHand.has_value() ? inHand->item.toString() : std::string{});
                    event.set("count", static_cast<std::int64_t>(inHand.has_value() ? inHand->count : 0));
                    event.set("stand_item", onStand.has_value() ? onStand->item.toString() : std::string{});
                    event.set("stand_count", static_cast<std::int64_t>(onStand.has_value() ? onStand->count : 0));
                })) {
            return false;
        }
        const auto sendHand = [&]() { owner_->sendInventorySlot(handIndex); };
        // parity: `Player.hasInfiniteMaterials` fills an empty slot without spending the stack.
        if (session.gamemode == Gamemode::Creative && !onStand.has_value() && inHand.has_value()) {
            ItemStack one = *inHand;
            one.count = 1;
            stand.set(slot, one);
            return true;
        }
        if (inHand.has_value() && inHand->count > 1) {
            if (onStand.has_value()) {
                return false;
            }
            ItemStack one = *inHand;
            one.count = 1;
            stand.set(slot, one);
            storage::StoredItemStack left{
                .id = inHand->item.toString(), .count = inHand->count - 1, .components = inHand->components};
            session.inventory.set(handIndex, left);
            sendHand();
            return true;
        }
        stand.set(slot, inHand);
        std::optional<storage::StoredItemStack> back;
        if (onStand.has_value()) {
            back = storage::StoredItemStack{
                .id = onStand->item.toString(), .count = onStand->count, .components = onStand->components};
        }
        session.inventory.set(handIndex, back);
        sendHand();
        return true;
    };
    if (!inHand.has_value()) {
        const Slot clicked = stand.clickedSlot(clickY);
        const Slot target = stand.isDisabled(clicked) ? itemSlot : clicked;
        if (stand.has(target) && swap(target)) {
            return ArmorStandUse::Done;
        }
        return ArmorStandUse::Pass;
    }
    if (stand.isDisabled(itemSlot) || (isHand(itemSlot) && !stand.showArms())) {
        return ArmorStandUse::Done;
    }
    return swap(itemSlot) ? ArmorStandUse::Done : ArmorStandUse::Pass;
}

} // namespace basalt::server::runtime
