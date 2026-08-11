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
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/PlayerThrown.hpp>
#include <basalt/server/runtime/player/PlayerTrident.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
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
using wire::protocol::play::SoundSource;

constexpr std::string_view kBow = "bow";
constexpr std::string_view kCrossbow = "crossbow";

constexpr std::string_view kShield = "shield";
constexpr std::string_view kSpyglass = "spyglass";
/// parity: `SpyglassItem.USE_DURATION`, a minute.
constexpr std::int32_t kSpyglassUseTicks = 1200;

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
    // `use_item_on` and `use_item` are one press; restarting the timer on the second never let a held button finish.
    if (eatingSlot_ == slot) {
        return true;
    }
    if (hooks_.consumeAllowed && !hooks_.consumeAllowed(session, item)) {
        owner_->sendInventory();
        return false;
    }
    // parity: only a `minecraft:food` component can refuse a full stomach; a bucket of milk or a bottle never does.
    if (food->nutrition > 0 && !food->canAlwaysEat && session.food.foodLevel() >= player::FoodData::kMaxFood) {
        // parity: `ServerPlayerGameMode.useItem`.
        owner_->sendInventoryAll();
        owner_->syncHealth();
        return true;
    }
    eatingSlot_ = slot;
    eatingTicks_ = food->consumeTicks();
    eatingDuration_ = eatingTicks_;
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
    const std::optional<world::TriggerItem> item = world::triggerFacts::itemOf(eaten);
    if (triggers != nullptr && item.has_value()) {
        triggers->fire("consume_item", session.name(), session.uuid().toString(),
                       world::TriggerEvent{.items = {{"item", *item}}});
    }
}

namespace {
/// parity: `Item.getUseDuration` of a bow, a crossbow, a trident and a shield.
constexpr std::int32_t kHeldUseTicks = 72000;
} // namespace

std::optional<ItemUseTimers::UseState> ItemUseTimers::useState() const {
    if (eatingSlot_ >= 0) {
        return UseState{.used = eatingDuration_ - eatingTicks_, .duration = eatingDuration_};
    }
    if (spyglassSlot_ >= 0) {
        return UseState{.used = kSpyglassUseTicks - spyglassTicks_, .duration = kSpyglassUseTicks};
    }
    if (drawingSlot_ >= 0) {
        return UseState{.used = drawingTicks_, .duration = kHeldUseTicks};
    }
    const PlayerSession* session = owner_->playerSession();
    if (session != nullptr && session->blockingSlot.has_value()) {
        return UseState{.used = session->blockingTicks, .duration = kHeldUseTicks};
    }
    return std::nullopt;
}

bool ItemUseTimers::setRemaining(std::int32_t ticks) {
    ticks = std::max(ticks, 1);
    if (eatingSlot_ >= 0) {
        eatingTicks_ = ticks;
        return true;
    }
    if (spyglassSlot_ >= 0) {
        spyglassTicks_ = ticks;
        return true;
    }
    if (drawingSlot_ >= 0) {
        drawingTicks_ = std::max(kHeldUseTicks - ticks, 0);
        return true;
    }
    if (PlayerSession* session = owner_->playerSession(); session != nullptr && session->blockingSlot.has_value()) {
        session->blockingTicks = std::max(kHeldUseTicks - ticks, 0);
        return true;
    }
    return false;
}

void ItemUseTimers::completeUse(PlayerSession& session) {
    if (eatingSlot_ >= 0) {
        eatingTicks_ = 1;
        tickEating(session);
    } else if (drawingSlot_ >= 0) {
        releaseBow();
    } else if (spyglassSlot_ >= 0) {
        stopSpyglass();
    } else if (session.blockingSlot.has_value()) {
        stopBlocking();
    }
}

void ItemUseTimers::clearUse() {
    if (eatingSlot_ >= 0) {
        stopEating();
    }
    if (drawingSlot_ >= 0) {
        stopDrawing();
    }
    if (spyglassSlot_ >= 0) {
        spyglassSlot_ = -1;
        spyglassTicks_ = 0;
        setUsingItem(false);
    }
    stopBlocking();
}

bool ItemUseTimers::startUse(PlayerSession& session, std::int32_t slot) {
    const std::optional<storage::StoredItemStack>& held = session.inventory.at(slot);
    const std::optional<Identifier> id = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    if (!id.has_value() || useState().has_value()) {
        return false;
    }
    return startEating(session, *id, slot) || startDrawing(session, *id, slot) || startBlocking(session, *id, slot) ||
           startSpyglass(session, *id, slot);
}

bool ItemUseTimers::startDrawing(PlayerSession& session, const Identifier& item, std::int32_t slot) {
    // parity: `CrossbowItem.use`.
    if (item.path() == kCrossbow) {
        const std::optional<storage::StoredItemStack>& held = session.inventory.at(slot);
        world::LoadedWorld* world = owner_->currentWorld();
        if (held.has_value() && player::crossbow::isCharged(*held)) {
            if (world != nullptr) {
                player::crossbow::shoot(owner_->blockInteraction().breakContext(), owner_->options().fireworks, session,
                                        *world, slot);
            }
            return true;
        }
        if (!player::crossbow::ammoSlot(session).has_value() && session.gamemode != Gamemode::Creative) {
            return true;
        }
        crossbowCharging_ = {};
        drawingSlot_ = slot;
        drawingTicks_ = 0;
        setUsingItem(true, item, slot == PlayerInventory::kOffhandSlot);
        return true;
    }
    // parity: `TridentItem.use`, raised for as long as it is held and thrown on release.
    if (item.path() == "trident") {
        const std::optional<storage::StoredItemStack>& held = session.inventory.at(slot);
        world::LoadedWorld* world = owner_->currentWorld();
        if (held.has_value() && world != nullptr &&
            player::trident::canRaise(owner_->blockInteraction().breakContext(), *world, session, *held)) {
            drawingSlot_ = slot;
            drawingTicks_ = 0;
            setUsingItem(true, item, slot == PlayerInventory::kOffhandSlot);
        }
        return true;
    }
    if (item.path() != kBow) {
        return false;
    }
    // parity: `BowItem.use`.
    if (session.gamemode != Gamemode::Creative && !session.inventory.arrowSlot().has_value()) {
        return true;
    }
    drawingSlot_ = slot;
    drawingTicks_ = 0;
    setUsingItem(true, item, slot == PlayerInventory::kOffhandSlot);
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
    if (!id.has_value() || (id->path() != kBow && id->path() != kCrossbow && id->path() != "trident") ||
        (drawingSlot_ != PlayerInventory::kOffhandSlot &&
         drawingSlot_ != PlayerInventory::kHotbarStart + session.inventory.heldSlot())) {
        stopDrawing();
        return;
    }
    // parity: `CrossbowItem.onUseTick`, handed the ticks held before this one.
    if (id->path() == kCrossbow) {
        if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
            player::crossbow::tickCharging(owner_->blockInteraction().breakContext(), session, *world, drawingSlot_,
                                           drawingTicks_, crossbowCharging_);
        }
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
    const std::optional<storage::StoredItemStack>& drawnItem = session->inventory.at(slot);
    // parity: `CrossbowItem.releaseUsing` only ends the use.
    const bool crossbow = drawnItem.has_value() && player::crossbow::isCrossbow(*drawnItem);
    const bool trident = drawnItem.has_value() && player::trident::isTrident(*drawnItem);
    stopDrawing();
    if (world != nullptr && trident) {
        player::trident::release(owner_->blockInteraction().breakContext(), *session, *world, slot, drawn);
        return;
    }
    if (world != nullptr && !crossbow) {
        player::shootBowFor(owner_->blockInteraction().breakContext(), *session, *world, slot, drawn,
                            hooks_.shootAllowed);
    }
}

bool ItemUseTimers::throwEnderPearl(PlayerSession& session, const Identifier& item, std::int32_t slot) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr || !player::isThrowable(item)) {
        return player::isThrowable(item);
    }
    if (!player::throwItemFor(owner_->blockInteraction().breakContext(), session, *world, item, slot)) {
        return false;
    }
    startCooldown(item);
    return true;
}

bool ItemUseTimers::startSpyglass(PlayerSession& session, const Identifier& item, std::int32_t slot) {
    if (item.path() != kSpyglass) {
        return false;
    }
    playAroundExceptSelf(session, "item.spyglass.use");
    player::statAwards::used(session, item.toString());
    spyglassSlot_ = slot;
    spyglassTicks_ = kSpyglassUseTicks;
    setUsingItem(true, item, slot == PlayerInventory::kOffhandSlot);
    return true;
}

void ItemUseTimers::stopSpyglass() {
    if (spyglassSlot_ < 0) {
        return;
    }
    spyglassSlot_ = -1;
    spyglassTicks_ = 0;
    if (PlayerSession* session = owner_->playerSession(); session != nullptr) {
        playAroundExceptSelf(*session, "item.spyglass.stop_using");
    }
    setUsingItem(false);
}

void ItemUseTimers::tickSpyglass(PlayerSession& session) {
    if (spyglassSlot_ < 0) {
        return;
    }
    const std::optional<storage::StoredItemStack>& held = session.inventory.at(spyglassSlot_);
    const bool inHand = spyglassSlot_ == PlayerInventory::kOffhandSlot ||
                        spyglassSlot_ == PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    if (!held.has_value() || held->id != "minecraft:spyglass" || !inHand) {
        // parity: `stopUsingItem` on a changed hand ends it without `releaseUsing`, so silently.
        spyglassSlot_ = -1;
        spyglassTicks_ = 0;
        setUsingItem(false);
        return;
    }
    // parity: `finishUsingItem` at the end of the minute, which stops it with the same sound.
    if (--spyglassTicks_ <= 0) {
        stopSpyglass();
    }
}

void ItemUseTimers::tickUsingTrigger(PlayerSession& session) {
    world::AdvancementTriggers* triggers = owner_->options().advancementTriggers;
    if (triggers == nullptr || !usingItem_ || !usedItem_.has_value() || !triggers->listensTo("using_item")) {
        return;
    }
    const std::int32_t mainHand = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    const std::string used = usedItem_->toString();
    const std::optional<storage::StoredItemStack>& main = session.inventory.at(mainHand);
    const std::optional<storage::StoredItemStack>& stack =
        main.has_value() && main->id == used ? main : session.inventory.at(PlayerInventory::kOffhandSlot);
    if (const std::optional<world::TriggerItem> item = world::triggerFacts::itemOf(stack); item.has_value()) {
        triggers->fire("using_item", session.name(), session.uuid().toString(),
                       world::TriggerEvent{.items = {{"item", *item}}});
    }
}

void ItemUseTimers::playAroundExceptSelf(const PlayerSession& session, std::string_view sound) {
    // parity: `Player.playSound`.
    const std::int64_t seed = options_.nextSeed ? options_.nextSeed() : 0;
    for (const PlayerSession* other : owner_->playersInSameWorld()) {
        if (other->entityId() != session.entityId()) {
            player::playSoundFor(*other, sound, wire::protocol::play::SoundSource::Players, session.x, session.y,
                                 session.z, 1.0F, 1.0F, seed);
        }
    }
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
                            world::triggerFacts::vibrationSourceOf(*session, owner_->options().advancementTriggers));
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
    // parity: `Item.Properties.spear`.
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
    // parity: `startUsingItem`.
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
    // The tracker sends living flags to the owner too, whose `isUsingItem` clears only from that metadata.
    session->usingItem = usingItem;
    session->usingOffhand = usingItem && offhand;
}

} // namespace basalt::server::runtime
