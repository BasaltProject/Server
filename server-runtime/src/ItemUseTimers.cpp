#include <basalt/server/registry/ItemFood.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ItemUseTimers.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerCooldowns.hpp>
#include <basalt/server/runtime/player/PlayerEating.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerThrown.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>

namespace basalt::server::runtime {
namespace {

using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::Gamemode;
using wire::protocol::play::SetEntityDataPacket;
using wire::protocol::play::SoundSource;

constexpr std::string_view kBow = "bow";
constexpr std::string_view kArrowItem = "minecraft:arrow";
constexpr std::string_view kEnderPearl = "ender_pearl";

/// parity: `LivingEntity`'s own using-item bit.
constexpr std::int8_t kLivingFlagUsingItem = 1;
/// parity: `LivingEntity.LIVING_ENTITY_FLAG_OFF_HAND`, which hand the item in use is in.
constexpr std::int8_t kLivingFlagOffhand = 2;
constexpr std::string_view kShield = "shield";

/// A stored stack's id as an identifier, or nothing when the row holds something unparseable.
std::optional<Identifier> parsedOrNothing(const std::string& id) {
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

} // namespace

ItemUseTimers::ItemUseTimers(ClientSession& owner, Hooks hooks, Options options)
    : owner_(&owner), hooks_(std::move(hooks)), options_(std::move(options)) {}

bool ItemUseTimers::startEating(PlayerSession& session, const Identifier& item, std::int32_t slot) {
    const server::registry::FoodValue* food = server::registry::itemFood::of(item);
    if (food == nullptr) {
        return false;
    }
    // A click on a block and the click that follows it are one mouthful, not two: the client sends
    // `use_item_on` and then `use_item` for the same press, and restarting the timer on the second
    // one meant a held button never finished a single one.
    if (eatingSlot_ == slot) {
        return true;
    }
    if (hooks_.consumeAllowed && !hooks_.consumeAllowed(session, item)) {
        owner_->sendInventory();
        return false;
    }
    if (!food->canAlwaysEat && session.food.foodLevel() >= player::FoodData::kMaxFood) {
        // parity: `ServerPlayerGameMode.useItem` sends the whole inventory back whenever the use
        // never started. Without it a client that had already predicted the mouthful kept the
        // animation and the missing item for good.
        owner_->sendInventoryAll();
        owner_->syncHealth();
        return true;
    }
    eatingSlot_ = slot;
    eatingTicks_ = food->consumeTicks();
    setUsingItem(true, item);
    return true;
}

void ItemUseTimers::stopEating() {
    eatingSlot_ = -1;
    eatingTicks_ = 0;
    setUsingItem(false);
}

void ItemUseTimers::tickEating(PlayerSession& session) {
    if (eatingSlot_ < 0) {
        return;
    }
    const std::optional<storage::StoredItemStack>& held = session.inventory.at(eatingSlot_);
    const server::registry::FoodValue* food = nullptr;
    if (held.has_value()) {
        const std::optional<Identifier> id = parsedOrNothing(held->id);
        food = id.has_value() ? server::registry::itemFood::of(*id) : nullptr;
    }
    // Anything that moved the stack under them (dropping it, a click, swapping slots) stops it.
    if (food == nullptr || eatingSlot_ != PlayerInventory::kHotbarStart + session.inventory.heldSlot()) {
        stopEating();
        return;
    }
    if (--eatingTicks_ > 0) {
        return;
    }
    const std::int32_t slot = eatingSlot_;
    stopEating();
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    const std::optional<storage::StoredItemStack> eaten = session.inventory.at(slot);
    if (!player::eatHeldItem(owner_->blockInteraction().breakContext(), session, *world, slot,
                             [this] { return options_.nextFloat ? options_.nextFloat() : 0.5F; })) {
        return;
    }
    // parity: `Consumable.onConsume` ends in `CONSUME_ITEM.trigger` with the stack that was eaten.
    world::AdvancementTriggers* triggers = owner_->options().advancementTriggers;
    const std::optional<Identifier> item = eaten.has_value() ? parsedOrNothing(eaten->id) : std::nullopt;
    if (triggers != nullptr && item.has_value()) {
        triggers->onConsumedItem(session.name(), session.uuid().toString(), *item);
    }
}

bool ItemUseTimers::startDrawing(PlayerSession& session, const Identifier& item, std::int32_t slot) {
    if (item.path() != kBow) {
        return false;
    }
    // parity: `BowItem.use`, which refuses without an arrow unless the player has infinite
    // materials, and otherwise just starts the draw: the shot happens on release.
    if (session.gamemode != Gamemode::Creative && !session.inventory.indexOf(kArrowItem).has_value()) {
        return true;
    }
    drawingSlot_ = slot;
    drawingTicks_ = 0;
    setUsingItem(true, item);
    return true;
}

void ItemUseTimers::stopDrawing() {
    drawingSlot_ = -1;
    drawingTicks_ = 0;
    setUsingItem(false);
}

void ItemUseTimers::tickDrawing(PlayerSession& session) {
    if (drawingSlot_ < 0) {
        return;
    }
    const std::optional<storage::StoredItemStack>& held = session.inventory.at(drawingSlot_);
    const std::optional<Identifier> id = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    if (!id.has_value() || id->path() != kBow ||
        drawingSlot_ != PlayerInventory::kHotbarStart + session.inventory.heldSlot()) {
        stopDrawing();
        return;
    }
    drawingTicks_++;
}

float ItemUseTimers::bowPower(std::int32_t ticks) { return player::bowPowerOf(ticks); }

void ItemUseTimers::releaseBow() {
    PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || drawingSlot_ < 0) {
        return;
    }
    const std::int32_t slot = drawingSlot_;
    const std::int32_t drawn = drawingTicks_;
    stopDrawing();
    if (world != nullptr) {
        player::shootBowFor(owner_->blockInteraction().breakContext(), *session, *world, slot, drawn,
                            hooks_.shootAllowed);
    }
}

bool ItemUseTimers::throwEnderPearl(PlayerSession& session, const Identifier& item, std::int32_t slot) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr || item.path() != kEnderPearl) {
        return item.path() == kEnderPearl;
    }
    if (!player::throwItemFor(owner_->blockInteraction().breakContext(), session, *world, item, slot)) {
        return false;
    }
    startCooldown(item);
    return true;
}

bool ItemUseTimers::onCooldown(const Identifier& item) {
    PlayerSession* session = owner_->playerSession();
    const world::LoadedWorld* world = owner_->currentWorld();
    return session != nullptr && world != nullptr && player::onCooldownFor(*session, item, world->gameTime());
}

void ItemUseTimers::startCooldown(const Identifier& item) {
    PlayerSession* session = owner_->playerSession();
    const world::LoadedWorld* world = owner_->currentWorld();
    if (session != nullptr && world != nullptr) {
        player::startCooldownFor(*session, item, world->gameTime());
    }
}

void ItemUseTimers::emitAt(std::string_view event) {
    PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || world == nullptr) {
        return;
    }
    world::vibrations::emit(world->state(), event, session->x, session->y, session->z, world->gameTime(),
                            {.entity = session->uuid()});
}

bool ItemUseTimers::startBlocking(PlayerSession& session, const Identifier& item, std::int32_t slot) {
    if (item.path() != kShield) {
        return false;
    }
    session.blockingSlot = slot;
    session.blockingTicks = 0;
    setUsingItem(true, item, slot == PlayerInventory::kOffhandSlot);
    return true;
}

void ItemUseTimers::stopBlocking() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr || !session->blockingSlot.has_value()) {
        return;
    }
    session->blockingSlot.reset();
    session->blockingTicks = 0;
    setUsingItem(false);
}

void ItemUseTimers::tickBlocking(PlayerSession& session) {
    if (!session.blockingSlot.has_value()) {
        return;
    }
    const std::int32_t slot = *session.blockingSlot;
    const std::optional<storage::StoredItemStack>& held = session.inventory.at(slot);
    const bool inHand =
        slot == PlayerInventory::kOffhandSlot || slot == PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    if (!held.has_value() || held->id != "minecraft:shield" || !inHand) {
        stopBlocking();
        return;
    }
    ++session.blockingTicks;
}

void ItemUseTimers::raiseUseVibration(std::string_view event, const Identifier& item) {
    // parity: `Item.Properties.spear` is the one builder that turns `interact_vibrations` off; every
    // other item carries the common component, whose default is on.
    if (item.path().ends_with("spear")) {
        return;
    }
    emitAt(event);
}

void ItemUseTimers::setUsingItem(bool usingItem, std::optional<Identifier> item, bool offhand) {
    if (usingItem_ == usingItem) {
        return;
    }
    usingItem_ = usingItem;
    // parity: `startUsingItem` and `stopUsingItem` each end in `causeUseVibration`, and the second
    // only when something really was being used, which is what the check above already is.
    if (usingItem && item.has_value()) {
        usedItem_ = item;
        raiseUseVibration("item_interact_start", *item);
    } else if (!usingItem && usedItem_.has_value()) {
        raiseUseVibration("item_interact_finish", *usedItem_);
        usedItem_.reset();
    }
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    // Kept on the session as well as sent: the door with no metadata of its own reads it from here
    // to draw somebody else eating.
    session->usingItem = usingItem;
    // The owner's own client needs this as much as the onlookers do: `LocalPlayer.isUsingItem` is
    // its own flag, and the only thing that clears it is this metadata coming back saying the
    // server is no longer using anything.
    const SetEntityDataPacket packet{
        .entityId = session->entityId(),
        .entries = {SetEntityDataPacket::Entry{
            .index = SetEntityDataPacket::kLivingFlagsIndex,
            .value = SetEntityDataPacket::ByteValue{
                .value = static_cast<std::int8_t>((usingItem ? kLivingFlagUsingItem : 0) |
                                                  (usingItem && offhand ? kLivingFlagOffhand : 0))}}}};
    const auto write = [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); };
    owner_->sendPlayPacket(SetEntityDataPacket::kId, write);
    for (PlayerSession* other : owner_->playersInSameWorld()) {
        if (other->entityId() == session->entityId() || !other->connection().sendPlayPacket) {
            continue;
        }
        other->connection().sendPlayPacket(SetEntityDataPacket::kId, write);
    }
}

} // namespace basalt::server::runtime
