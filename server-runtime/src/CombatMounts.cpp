#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/Camels.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using entity::mobEquipment::Slot;
using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::Gamemode;
using wire::protocol::play::SoundSource;
namespace horses = entity::horses;

constexpr std::string_view kMaxHealth = "minecraft:max_health";

[[nodiscard]] std::optional<Identifier> itemOf(const std::optional<storage::StoredItemStack>& stack) {
    if (!stack.has_value() || stack->count <= 0) {
        return std::nullopt;
    }
    const auto parsed = Identifier::parse(stack->id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

} // namespace

void CombatHandler::mountSound(const ServerEntity& mount, std::string_view sound, float volume, float pitch) {
    if (!sound.empty() && !horses::silent(mount)) {
        owner_->broadcastSound(sound, mount.x, mount.y, mount.z, SoundSource::Neutral, volume, pitch);
    }
}

float CombatHandler::soundRoll() const {
    const double first = options_.nextDouble ? options_.nextDouble() : 0.5;
    const double second = options_.nextDouble ? options_.nextDouble() : 0.5;
    return static_cast<float>(first) - static_cast<float>(second);
}

void CombatHandler::equipMount(ServerEntity& mount, Slot slot,
                               const std::optional<server::world::loot::ItemStack>& stack) {
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<server::world::loot::ItemStack> before = entity::mobEquipment::get(mount, slot);
    // parity: `Mob.createEquipmentSlotContainer`: guaranteed to drop, and the mob kept.
    entity::mobEquipment::set(mount, slot, stack, stack.has_value());
    if (stack.has_value()) {
        mount.persistent = true;
    }
    const server::registry::Item* item =
        stack.has_value() ? server::registry::items::registry().byName(stack->item) : nullptr;
    using wire::protocol::play::SetEquipmentPacket;
    SetEquipmentPacket::Slot entry{.type = slot == Slot::Saddle ? SetEquipmentPacket::SlotType::Saddle
                                                                : SetEquipmentPacket::SlotType::Body};
    if (item != nullptr) {
        entry.stack = wire::protocol::play::WireItemStack{.itemId = item->protocolId, .count = stack->count};
    }
    const SetEquipmentPacket packet{.entityId = mount.entityId(), .slots = {entry}};
    for (PlayerSession* watcher : owner_->playersInSameWorld()) {
        if (watcher->connection().sendPlayPacket) {
            watcher->connection().sendPlayPacket(SetEquipmentPacket::kId,
                                                 [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
    const bool same = before.has_value() == stack.has_value() &&
                      (!stack.has_value() || (before->item == stack->item && before->components == stack->components));
    if (same || world == nullptr) {
        return;
    }
    // parity: `LivingEntity.onEquipItem`: the equippable's sound, `EQUIP` or `UNEQUIP`.
    const bool wearable = stack.has_value() && horses::wearableFor(mount, stack->item) != horses::Wearable::None;
    if (wearable) {
        std::string_view sound = horses::saddleSound(mount);
        if (slot == Slot::Body) {
            sound = horses::isLlama(mount.type()) ? "minecraft:entity.llama.swag" : "minecraft:entity.horse.armor";
        }
        mountSound(mount, sound, 1.0F, 1.0F);
    }
    world::vibrations::emit(world->state(), wearable ? "equip" : "unequip", mount.x, mount.y, mount.z,
                            world->gameTime(), {.entity = mount.uuid()});
}

bool CombatHandler::feedMount(PlayerSession& session, ServerEntity& horse, const Identifier& item) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return false;
    }
    const double max = horse.attributeWithModifiers(kMaxHealth);
    bool used = false;
    const auto ageUp = [&horse](std::int32_t seconds) { horse.age = std::min(0, horse.age + (seconds * 20)); };
    if (horses::isCamel(horse.type())) {
        // parity: `Camel.handleEating`: two health, love when grown, ten seconds off a calf.
        const bool couldHeal = horse.health < max;
        if (couldHeal) {
            horse.health = std::min(max, horse.health + 2.0);
        }
        const bool couldLove = horse.age == 0 && entity::breeding::canFallInLove(horse);
        if (couldLove) {
            entity::breeding::feed(horse, session.uuid().toString());
        }
        const bool couldAgeUp = entity::breeding::canAgeUp(horse);
        if (couldAgeUp) {
            ageUp(10);
        }
        used = couldHeal || couldLove || couldAgeUp;
        if (used) {
            mountSound(horse, horses::eatingSound(horse), 1.0F, 1.0F + (soundRoll() * 0.2F));
            world::vibrations::emit(world->state(), "eat", horse.x, horse.y, horse.z, world->gameTime(),
                                    {.entity = horse.uuid()});
        }
        return used;
    }
    const horses::Feeding feeding = horses::feedingOf(horse.type(), item);
    const bool llama = horses::isLlama(horse.type());
    if (feeding.love && horses::tamed(horse) && horse.age == 0 && entity::breeding::canFallInLove(horse)) {
        used = true;
        entity::breeding::feed(horse, session.uuid().toString());
    }
    if (horse.health < max && feeding.heal > 0.0F) {
        horse.health = std::min(max, horse.health + static_cast<double>(feeding.heal));
        used = true;
    }
    if (horses::baby(horse) && feeding.ageUp > 0) {
        ageUp(feeding.ageUp);
        used = true;
    }
    if (feeding.temper > 0 && (used || !horses::tamed(horse)) && horses::temper(horse) < horses::maxTemper(horse)) {
        static_cast<void>(horses::modifyTemper(horse, feeding.temper));
        used = true;
    }
    if (!used) {
        return false;
    }
    // parity: `AbstractHorse.eating`, and the `EAT` a llama leaves out.
    if (!llama) {
        horses::openMouth(horse);
    }
    mountSound(horse, horses::eatingSound(horse), 1.0F, 1.0F + (soundRoll() * 0.2F));
    if (!llama) {
        world::vibrations::emit(world->state(), "eat", horse.x, horse.y, horse.z, world->gameTime(),
                                {.entity = horse.uuid()});
    }
    return true;
}

void CombatHandler::rideMount(PlayerSession& session, ServerEntity& horse) {
    // parity: `doPlayerRide`: it stops grazing and rearing and the player gets on.
    horses::HorseState& state = horses::stateOf(horse);
    state.eating = false;
    state.standing = false;
    state.standCounter = 0;
    mount(session, horse);
}

bool CombatHandler::openMountInventory(PlayerSession& session, ServerEntity& horse) {
    // parity: `openCustomInventoryScreen`.
    const bool riding = std::ranges::find(horse.passengerIds, session.entityId()) != horse.passengerIds.end();
    if (!horses::isCamel(horse.type()) && ((!horse.passengerIds.empty() && !riding) || !horses::tamed(horse))) {
        return false;
    }
    owner_->menus().openMountMenu(session, horse);
    return true;
}

bool CombatHandler::interactMount(PlayerSession& session, ServerEntity& horse, bool mainHand) {
    const std::int32_t slot =
        mainHand ? PlayerInventory::kHotbarStart + session.inventory.heldSlot() : PlayerInventory::kOffhandSlot;
    const std::optional<storage::StoredItemStack> held = session.inventory.at(slot);
    const std::optional<Identifier> item = itemOf(held);
    const bool baby = horses::baby(horse);
    const bool vehicle = !horse.passengerIds.empty();
    const std::string_view path = horse.type().path();
    const bool creative = session.gamemode == Gamemode::Creative;
    const auto consumeOne = [&](bool keepInCreative) {
        if (!held.has_value() || (keepInCreative && creative)) {
            return;
        }
        storage::StoredItemStack left = *held;
        left.count -= 1;
        session.inventory.set(slot, left.count <= 0 ? std::nullopt : std::optional{left});
        owner_->sendInventorySlot(slot);
    };
    const auto interacted = [&] {
        world::LoadedWorld* world = owner_->currentWorld();
        world::AdvancementTriggers* triggers = owner_->options().advancementTriggers;
        if (world != nullptr && triggers != nullptr && item.has_value()) {
            triggers->fire("player_interacted_with_entity", session.name(), session.uuid().toString(),
                           world::TriggerEvent{.entities = {{"entity", world::triggerFacts::ofEntity(*world, horse)}},
                                               .items = {{"item", world::triggerFacts::itemOf(held).value_or(
                                                                      world::TriggerItem{.item = *item})}}});
        }
        return true;
    };
    // parity: `Equippable.equipOnTarget` for a saddle.
    const auto equipSaddle = [&] {
        if (!item.has_value() || horses::wearableFor(horse, *item) != horses::Wearable::Saddle ||
            horses::saddled(horse) || horse.health <= 0.0) {
            return false;
        }
        equipMount(horse, Slot::Saddle,
                   server::world::loot::ItemStack{.item = *item, .count = 1, .components = held->components});
        consumeOne(false);
        return true;
    };
    MobActionSystem* mobs = owner_->options().mobs;
    if (horses::isCamel(horse.type())) {
        // parity: `Camel.mobInteract`.
        if (session.sneaking && !baby) {
            static_cast<void>(openMountInventory(session, horse));
            return true;
        }
        if (equipSaddle()) {
            return interacted();
        }
        if (item.has_value() && horses::isFood(horse.type(), *item)) {
            if (feedMount(session, horse, *item)) {
                consumeOne(true);
                return interacted();
            }
            return true;
        }
        if (horse.passengerIds.size() < 2 && !baby) {
            rideMount(session, horse);
            return true;
        }
        return !(baby && item.has_value() && item->path() == "golden_dandelion");
    }
    if (path == "skeleton_horse" && !horses::tamed(horse)) {
        return true;
    }
    // parity: `Horse`.
    const bool shouldOpen = !baby && horses::tamed(horse) && session.sneaking;
    const bool dandelion = item.has_value() && item->path() == "golden_dandelion";
    if (path != "skeleton_horse" && !vehicle && !shouldOpen && (!baby || !dandelion) && item.has_value()) {
        if (horses::isFood(horse.type(), *item)) {
            if (feedMount(session, horse, *item)) {
                consumeOne(true);
                return interacted();
            }
            return true;
        }
        if (!horses::tamed(horse)) {
            if (mobs != nullptr) {
                mobs->makeHorseMad(horse);
            }
            return interacted();
        }
        if (horses::isChested(horse.type()) && !horses::hasChest(horse) && item->path() == "chest") {
            horses::setChest(horse, true);
            mountSound(horse, horses::chestSound(horse), 1.0F, (soundRoll() * 0.2F) + 1.0F);
            consumeOne(true);
            return interacted();
        }
    }
    // parity: `AbstractHorse.mobInteract`.
    if (vehicle || baby) {
        return false;
    }
    if (horses::tamed(horse) && session.sneaking) {
        static_cast<void>(openMountInventory(session, horse));
        return true;
    }
    if (item.has_value()) {
        if (equipSaddle()) {
            return interacted();
        }
        if (horses::wearableFor(horse, *item) == horses::Wearable::Body &&
            !entity::mobEquipment::get(horse, Slot::Body).has_value()) {
            equipMount(horse, Slot::Body,
                       server::world::loot::ItemStack{.item = *item, .count = 1, .components = held->components});
            consumeOne(true);
            return interacted();
        }
    }
    rideMount(session, horse);
    return true;
}

void CombatHandler::handleRidingCommand(wire::protocol::play::PlayerCommandPacket::Action action, std::int32_t data) {
    using Action = wire::protocol::play::PlayerCommandPacket::Action;
    PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    ServerEntity* vehicle = session == nullptr || world == nullptr || !session->vehicleId.has_value()
                                ? nullptr
                                : world->state().entity(*session->vehicleId);
    if (vehicle == nullptr || !horses::isHorse(vehicle->type())) {
        return;
    }
    if (action == Action::OpenInventory) {
        static_cast<void>(openMountInventory(*session, *vehicle));
        return;
    }
    if (action != Action::StartRidingJump || data <= 0 || !controls(*session, *vehicle) || !horses::saddled(*vehicle)) {
        return;
    }
    if (horses::isCamel(vehicle->type())) {
        // parity: `Camel.canJump` and `handleStartJump`.
        if (entity::camels::refusesToMove(entity::camels::stateOf(*vehicle), world->gameTime())) {
            return;
        }
        mountSound(*vehicle,
                   vehicle->type().path() == "camel_husk" ? "minecraft:entity.camel_husk.dash"
                                                          : "minecraft:entity.camel.dash",
                   1.0F, horses::voicePitch(*vehicle, soundRoll()));
        world::vibrations::emit(world->state(), "entity_action", vehicle->x, vehicle->y, vehicle->z, world->gameTime(),
                                {.entity = vehicle->uuid()});
        horses::HorseState& state = horses::stateOf(*vehicle);
        state.dashing = true;
        if (state.dashCooldown == 0) {
            state.dashCooldown = horses::kDashCooldownTicks;
        }
        return;
    }
    // parity: `AbstractHorse.handleStartJump`: up on its hind legs and the jump sound.
    horses::standIfPossible(*vehicle);
    std::string_view sound = horses::jumpSound(*vehicle);
    const std::optional<std::int32_t> feet = world->state().blockState(
        static_cast<std::int32_t>(std::floor(vehicle->x)), static_cast<std::int32_t>(std::floor(vehicle->y)),
        static_cast<std::int32_t>(std::floor(vehicle->z)));
    if (vehicle->type().path() == "skeleton_horse" && feet.has_value() &&
        world::fluids::of(*feet) == world::Submersion::Water) {
        sound = "minecraft:entity.skeleton_horse.jump_water";
    }
    mountSound(*vehicle, sound, 0.4F, 1.0F);
}

} // namespace basalt::server::runtime
