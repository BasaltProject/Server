#include <basalt/server/registry/ItemFood.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ItemUseTimers.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>

#include <algorithm>
#include <cmath>
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
constexpr std::string_view kSoundArrowShoot = "entity.arrow.shoot";
constexpr std::string_view kSoundPearlThrow = "entity.ender_pearl.throw";

/// parity: `LivingEntity`'s own using-item bit.
constexpr std::int8_t kLivingFlagUsingItem = 1;

/// parity: `BowItem`, twenty ticks to a full draw and a tenth of it before anything flies.
constexpr float kBowDrawTicks = 20.0F;
constexpr float kMinBowPower = 0.1F;
constexpr float kBowPowerScale = 3.0F;
constexpr float kBowUncertainty = 1.0F;

/// parity: `EnderpearlItem.use`, thrown at one and a half with a degree of spread.
constexpr float kPearlPower = 1.5F;
constexpr float kPearlUncertainty = 1.0F;

/// parity: `Entity.getEyeHeight` for a standing player, which is where a throw leaves from.
constexpr double kThrowEyeHeight = 1.62;

constexpr float kHalfCircle = 180.0F;

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
    setUsingItem(true);
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
    const storage::StoredItemStack eaten = *held;
    stopEating();
    session.food.eat(food->nutrition, food->saturation);
    owner_->applyConsumeEffects(session, food->effects);
    if (session.gamemode != Gamemode::Creative) {
        const std::int32_t remaining = eaten.count - 1;
        // parity: `use_remainder` replaces what was eaten: the bowl, the bottle, the bucket.
        std::optional<storage::StoredItemStack> leftover;
        if (food->remainder.has_value()) {
            leftover = eaten;
            leftover->id = food->remainder->toString();
            leftover->count = 1;
        }
        if (remaining > 0) {
            storage::StoredItemStack kept = eaten;
            kept.count = remaining;
            session.inventory.set(slot, kept);
        } else {
            session.inventory.set(slot, leftover);
        }
        owner_->sendInventorySlot(slot);
        // A stack of two bowls of stew leaves the second bowl looking for a slot of its own, and on
        // the floor when there is none.
        if (remaining > 0 && leftover.has_value()) {
            if (session.inventory.add(*leftover) > 0) {
                const std::optional<Identifier> id = parsedOrNothing(leftover->id);
                if (id.has_value()) {
                    owner_->spawnDroppedItem(*id, 1, session.x, session.y + kThrowEyeHeight, session.z);
                }
            }
            owner_->sendInventory();
        }
    }
    owner_->syncHealth();
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
    setUsingItem(true);
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

float ItemUseTimers::bowPower(std::int32_t ticks) {
    const float drawn = static_cast<float>(ticks) / kBowDrawTicks;
    return std::min(((drawn * drawn) + (drawn * 2.0F)) / 3.0F, 1.0F);
}

void ItemUseTimers::releaseBow() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr || drawingSlot_ < 0) {
        return;
    }
    const std::optional<storage::StoredItemStack> held = session->inventory.at(drawingSlot_);
    const std::int32_t drawn = drawingTicks_;
    stopDrawing();
    const std::optional<Identifier> heldId = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    if (!heldId.has_value() || heldId->path() != kBow) {
        return;
    }
    const float power = bowPower(drawn);
    if (power < kMinBowPower) {
        return;
    }
    const bool creative = session->gamemode == Gamemode::Creative;
    const std::optional<std::int32_t> arrowSlot = session->inventory.indexOf(kArrowItem);
    if (!creative && !arrowSlot.has_value()) {
        return;
    }
    world::LoadedWorld* world = owner_->currentWorld();
    player::PlayerRegistry* registry = owner_->options().playerRegistry;
    const entity::projectiles::Kind& kind = entity::projectiles::arrow();
    const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(kind.type);
    if (world == nullptr || registry == nullptr || entry == nullptr) {
        return;
    }
    if (hooks_.shootAllowed && !hooks_.shootAllowed(*session, kind.type, power)) {
        return;
    }
    const float pitch = session->pitch * std::numbers::pi_v<float> / kHalfCircle;
    const float yaw = session->yaw * std::numbers::pi_v<float> / kHalfCircle;
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    entity::ServerEntity& arrow = entity::projectiles::spawn(
        world->state(), kind, registry->allocateEntityId(), entry->protocolId, session->x, session->y + kThrowEyeHeight,
        session->z, static_cast<double>(-std::sin(yaw) * std::cos(pitch)), static_cast<double>(-std::sin(pitch)),
        static_cast<double>(std::cos(yaw) * std::cos(pitch)), power * kBowPowerScale, kBowUncertainty, random);
    arrow.ownerId = session->entityId();
    arrow.critical = power >= 1.0F;
    const float roll = options_.nextFloat ? options_.nextFloat() : 0.5F;
    owner_->broadcastSound(kSoundArrowShoot, session->x, session->y, session->z, SoundSource::Players, 1.0F,
                           (1.0F / ((roll * 0.4F) + 1.2F)) + (power * 0.5F));
    if (creative || !arrowSlot.has_value()) {
        return;
    }
    const std::optional<storage::StoredItemStack>& stack = session->inventory.at(*arrowSlot);
    if (!stack.has_value()) {
        return;
    }
    if (stack->count <= 1) {
        session->inventory.set(*arrowSlot, std::nullopt);
    } else {
        storage::StoredItemStack left = *stack;
        left.count = stack->count - 1;
        session->inventory.set(*arrowSlot, left);
    }
    owner_->sendInventorySlot(*arrowSlot);
}

bool ItemUseTimers::throwEnderPearl(PlayerSession& session, const Identifier& item, std::int32_t slot) {
    if (item.path() != kEnderPearl) {
        return false;
    }
    world::LoadedWorld* world = owner_->currentWorld();
    player::PlayerRegistry* registry = owner_->options().playerRegistry;
    const entity::projectiles::Kind* kind = entity::projectiles::kindOf(Identifier::vanilla("ender_pearl"));
    const server::registry::EntityTypeEntry* entry =
        kind == nullptr ? nullptr : server::registry::entityTypes::of(kind->type);
    if (world == nullptr || registry == nullptr || entry == nullptr) {
        return true;
    }
    const float pitch = session.pitch * std::numbers::pi_v<float> / kHalfCircle;
    const float yaw = session.yaw * std::numbers::pi_v<float> / kHalfCircle;
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    entity::ServerEntity& pearl = entity::projectiles::spawn(
        world->state(), *kind, registry->allocateEntityId(), entry->protocolId, session.x, session.y + kThrowEyeHeight,
        session.z, static_cast<double>(-std::sin(yaw) * std::cos(pitch)), static_cast<double>(-std::sin(pitch)),
        static_cast<double>(std::cos(yaw) * std::cos(pitch)), kPearlPower, kPearlUncertainty, random);
    pearl.ownerId = session.entityId();
    startCooldown(item);
    const float roll = options_.nextFloat ? options_.nextFloat() : 0.5F;
    owner_->broadcastSound(kSoundPearlThrow, session.x, session.y, session.z, SoundSource::Neutral, 0.5F,
                           0.4F / ((roll * 0.4F) + 0.8F));
    if (session.gamemode == Gamemode::Creative) {
        return true;
    }
    const std::optional<storage::StoredItemStack>& held = session.inventory.at(slot);
    if (!held.has_value()) {
        return true;
    }
    if (held->count <= 1) {
        session.inventory.set(slot, std::nullopt);
    } else {
        storage::StoredItemStack left = *held;
        left.count = held->count - 1;
        session.inventory.set(slot, left);
    }
    owner_->sendInventorySlot(slot);
    return true;
}

bool ItemUseTimers::onCooldown(const Identifier& item) {
    const auto found = itemCooldowns_.find(item.path());
    if (found == itemCooldowns_.end()) {
        return false;
    }
    if (found->second > owner_->playTimeTicks) {
        return true;
    }
    itemCooldowns_.erase(found);
    return false;
}

void ItemUseTimers::startCooldown(const Identifier& item) {
    const std::int32_t ticks = server::registry::itemStacking::cooldownTicks(item);
    if (ticks <= 0) {
        return;
    }
    itemCooldowns_[std::string{item.path()}] = owner_->playTimeTicks + ticks;
    owner_->sendPlayPacket(wire::protocol::play::CooldownPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::CooldownPacket{.cooldownGroup = item.toString(), .durationTicks = ticks}.write(buffer);
    });
}

void ItemUseTimers::setUsingItem(bool usingItem) {
    if (usingItem_ == usingItem) {
        return;
    }
    usingItem_ = usingItem;
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    // The owner's own client needs this as much as the onlookers do: `LocalPlayer.isUsingItem` is
    // its own flag, and the only thing that clears it is this metadata coming back saying the
    // server is no longer using anything.
    const SetEntityDataPacket packet{
        .entityId = session->entityId(),
        .entries = {SetEntityDataPacket::Entry{
            .index = SetEntityDataPacket::kLivingFlagsIndex,
            .value = SetEntityDataPacket::ByteValue{.value = usingItem ? kLivingFlagUsingItem : std::int8_t{0}}}}};
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
