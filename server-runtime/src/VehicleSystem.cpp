#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/EntityPushSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/VehicleSystem.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/Minecarts.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/Menu.hpp>
#include <basalt/server/runtime/player/PlayerConnection.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ContainerDrops.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/Explosions.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;
namespace mth = wire::common::mth;
namespace carts = entity::minecarts;

/// parity: `VehicleEntity.hurtServer`.
constexpr float kDamagePerPoint = 10.0F;
constexpr float kBreakDamage = 40.0F;
constexpr std::int32_t kHurtTicks = 10;
/// parity: `MinecartTNT`'s entity event for a lit fuse.
constexpr std::int8_t kPrimeEvent = 70;
/// parity: the speed past which a tnt cart hit sideways, or broken, goes off.
constexpr double kTntSpeed = static_cast<double>(0.01F);

/// parity: `getDropItem`.
[[nodiscard]] Identifier dropItemOf(const ServerEntity& vehicle) {
    const std::string_view path = vehicle.type().path();
    if (path == "command_block_minecart" || path == "spawner_minecart") {
        return Identifier::vanilla("minecart");
    }
    return vehicle.type();
}

} // namespace

VehicleSystem::VehicleSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster,
                             ExplosionSystem& explosions, EntityPushSystem& pushes, MobActionSystem& mobs,
                             Options options)
    : players_(&players), broadcaster_(&broadcaster), explosions_(&explosions), pushes_(&pushes), mobs_(&mobs),
      options_(std::move(options)) {}

std::vector<PlayerSession*> VehicleSystem::watchersOf(const world::LoadedWorld& world) const {
    std::vector<PlayerSession*> watchers;
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }
    return watchers;
}

bool VehicleSystem::typeIn(const Identifier& damageType, std::string_view tag) const {
    if (options_.damageTypeTags == nullptr) {
        return false;
    }
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(tag);
    return parsed.has_value() && world::realDamageTypeTags::isIn(*options_.damageTypeTags, *parsed, damageType);
}

bool VehicleSystem::ignitesTnt(const Hit& hit) const {
    // parity: `MinecartTNT.damageSourceIgnitesTnt`.
    // when it is fire or an explosion.
    if (hit.burningProjectile || hit.burningArrow) {
        return true;
    }
    return typeIn(hit.type, "minecraft:is_fire") || typeIn(hit.type, "minecraft:is_explosion");
}

bool VehicleSystem::ignoresExplosion(world::LoadedWorld& world, bool sourceIsMob) {
    return sourceIsMob && !world.gameRules().get(world::gameRules::kMobGriefing);
}

void VehicleSystem::entityEvent(world::LoadedWorld& world, const ServerEntity& entity, std::int8_t event) {
    const wire::protocol::play::EntityEventPacket packet{.entityId = entity.entityId(), .eventId = event};
    for (PlayerSession* player : watchersOf(world)) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::EntityEventPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void VehicleSystem::sendPassengers(world::LoadedWorld& world, const ServerEntity& vehicle) {
    const wire::protocol::play::SetPassengersPacket packet{.vehicleId = vehicle.entityId(),
                                                           .passengerIds = vehicle.passengerIds};
    for (PlayerSession* player : watchersOf(world)) {
        if (player->connection().sendPassengers) {
            player->connection().sendPassengers(vehicle.entityId(), vehicle.passengerIds);
        } else if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::SetPassengersPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void VehicleSystem::ejectPassengers(world::LoadedWorld& world, ServerEntity& vehicle) {
    // parity: `Entity.ejectPassengers`, the last one first.
    const std::vector<std::int32_t> aboard = vehicle.passengerIds;
    bool changed = false;
    for (auto id = aboard.rbegin(); id != aboard.rend(); ++id) {
        bool wasPlayer = false;
        for (PlayerSession* player : watchersOf(world)) {
            if (player->entityId() == *id && player->connection().dismount) {
                player->connection().dismount();
                wasPlayer = true;
            }
        }
        if (wasPlayer) {
            continue;
        }
        if (ServerEntity* mob = world.state().entity(*id); mob != nullptr) {
            mob->vehicleId.reset();
            // parity: `Entity.removeVehicle`, three seconds before it may board again.
            mob->boardingCooldown = 60;
        }
        std::erase(vehicle.passengerIds, *id);
        changed = true;
    }
    if (changed) {
        sendPassengers(world, vehicle);
    }
}

bool VehicleSystem::board(world::LoadedWorld& world, ServerEntity& vehicle, ServerEntity& mob) {
    // parity: `Entity.startRiding`.
    // way round, and `canRide`'s boarding cooldown.
    const std::int32_t room = entity::boats::isBoat(vehicle.type()) ? entity::boats::maxPassengers(vehicle.type()) : 1;
    if (mob.vehicleId == vehicle.entityId() || static_cast<std::int32_t>(vehicle.passengerIds.size()) >= room ||
        mob.boardingCooldown > 0 || vehicle.vehicleId == mob.entityId()) {
        return false;
    }
    if (mob.vehicleId.has_value()) {
        if (ServerEntity* old = world.state().entity(*mob.vehicleId); old != nullptr) {
            std::erase(old->passengerIds, mob.entityId());
            sendPassengers(world, *old);
        }
    }
    // parity: `Mob.startRiding` lets go of a lead.
    if (mob.leashHolderId.has_value()) {
        mobs_->dropLeash(world, mob, true);
    }
    mob.vehicleId = vehicle.entityId();
    vehicle.passengerIds.push_back(mob.entityId());
    sendPassengers(world, vehicle);
    return true;
}

bool VehicleSystem::hurt(world::LoadedWorld& world, ServerEntity& vehicle, const Hit& hit) {
    if (vehicle.removed) {
        return true;
    }
    const bool invulnerable = vehicle.sourceNbt.has_value() && vehicle.sourceNbt->getBoolean("Invulnerable");
    if (invulnerable && !hit.creativePlayer && !typeIn(hit.type, "minecraft:bypasses_invulnerability")) {
        return false;
    }
    const bool tnt = vehicle.type().path() == "tnt_minecart";
    // parity: `MinecartTNT.hurtServer`, a burning arrow sets it off before anything else.
    if (tnt && hit.burningArrow) {
        explodeTnt(world, vehicle, hit.arrowSpeedSqr);
        if (vehicle.removed) {
            return true;
        }
    }
    vehicle.vehicleHurtDir = -vehicle.vehicleHurtDir;
    vehicle.vehicleHurtTime = kHurtTicks;
    vehicle.vehicleDamage += hit.amount * kDamagePerPoint;
    std::optional<wire::common::Uuid> cause;
    if (hit.causeId.has_value()) {
        if (const ServerEntity* mob = world.state().entity(*hit.causeId); mob != nullptr) {
            cause = mob->uuid();
        }
        for (const PlayerSession* player : watchersOf(world)) {
            if (player->entityId() == *hit.causeId) {
                cause = player->uuid();
            }
        }
    }
    world::vibrations::emit(world.state(), "entity_damage", vehicle.x, vehicle.y, vehicle.z, world.gameTime(),
                            {.entity = cause});
    const bool sourceDestroys = tnt && ignitesTnt(hit);
    if ((hit.creativePlayer || vehicle.vehicleDamage <= kBreakDamage) && !sourceDestroys) {
        if (hit.creativePlayer) {
            // parity: `discard`, which still spills a container but drops no item.
            Hit discard = hit;
            destroy(world, vehicle, discard);
        }
        return true;
    }
    destroy(world, vehicle, hit);
    return true;
}

void VehicleSystem::destroy(world::LoadedWorld& world, ServerEntity& vehicle, const Hit& hit) {
    // parity: `MinecartTNT.destroy`.
    if (vehicle.type().path() == "tnt_minecart" && !hit.creativePlayer) {
        const double speedSqr = (vehicle.velocityX * vehicle.velocityX) + (vehicle.velocityZ * vehicle.velocityZ);
        if (ignitesTnt(hit) || speedSqr >= kTntSpeed) {
            if (vehicle.fuse < 0) {
                primeTnt(world, vehicle, hit.causeId);
                random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
                vehicle.fuse = random.nextInt(20) + random.nextInt(20);
            }
            return;
        }
    }
    if (!hit.creativePlayer) {
        // parity: `VehicleEntity.destroy`: killed, then its item with its name, if entities drop.
        world::vibrations::emit(world.state(), "entity_die", vehicle.x, vehicle.y, vehicle.z, world.gameTime(),
                                {.entity = vehicle.uuid()});
        if (world.gameRules().get(world::gameRules::kEntityDrops)) {
            wire::nbt::NbtCompound components;
            if (const wire::nbt::NbtTag* name =
                    vehicle.sourceNbt.has_value() ? vehicle.sourceNbt->find("CustomName") : nullptr;
                name != nullptr) {
                components.set("minecraft:custom_name", *name);
            }
            random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
            // parity: `Entity.spawnAtLocation`, with the default pickup delay and a little hop.
            world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                       .uuid = wire::common::Uuid::random(),
                                                       .item = dropItemOf(vehicle),
                                                       .count = 1,
                                                       .x = vehicle.x,
                                                       .y = vehicle.y,
                                                       .z = vehicle.z,
                                                       .velocityX = (random.nextDouble() * 0.2) - 0.1,
                                                       .velocityY = 0.2,
                                                       .velocityZ = (random.nextDouble() * 0.2) - 0.1,
                                                       .pickupDelay = world::kSpawnedPickupDelay,
                                                       .components = std::move(components)});
        }
    }
    // parity: `AbstractChestBoat.remove` and `AbstractMinecartContainer.remove` spill what is inside.
    // whatever the rules say, a creative player's discard included.
    const std::string_view path = vehicle.type().path();
    const bool container =
        entity::boats::hasChest(vehicle.type()) || path == "chest_minecart" || path == "hopper_minecart";
    if (container && vehicle.sourceNbt.has_value()) {
        const std::int32_t size = path == "hopper_minecart" ? 5 : player::Menu::kChestSize;
        random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
        for (const std::optional<server::world::loot::ItemStack>& slot :
             world::blockInventory::read(*vehicle.sourceNbt, size)) {
            if (slot.has_value() && slot->count > 0) {
                world::containerDrops::dropItemStack(world.state(), vehicle.x, vehicle.y, vehicle.z, *slot, random,
                                                     [this] { return players_->allocateEntityId(); });
            }
        }
        static_cast<void>(vehicle.sourceNbt->remove("Items"));
    }
    ejectPassengers(world, vehicle);
    vehicle.removed = true;
}

void VehicleSystem::primeTnt(world::LoadedWorld& world, ServerEntity& cart, std::optional<std::int32_t> cause) {
    // parity: `MinecartTNT.primeFuse`, nothing at all with the `tnt_explodes` rule off.
    if (!world.gameRules().get(world::gameRules::kTntExplodes)) {
        return;
    }
    cart.fuse = carts::kTntCartFuse;
    carts::CartState& state = carts::stateOf(cart);
    if (cause.has_value() && !state.ignited) {
        state.ignitedBy = cause;
        state.ignited = true;
    }
    entityEvent(world, cart, kPrimeEvent);
    if (!(cart.sourceNbt.has_value() && cart.sourceNbt->getBoolean("Silent"))) {
        const std::int64_t seed = options_.nextSeed ? options_.nextSeed() : 0;
        for (const PlayerSession* player : watchersOf(world)) {
            player::playSoundFor(*player, "minecraft:entity.tnt.primed", wire::protocol::play::SoundSource::Blocks,
                                 cart.x, cart.y, cart.z, 1.0F, 1.0F, seed);
        }
    }
}

void VehicleSystem::explodeTnt(world::LoadedWorld& world, ServerEntity& cart, double speedSqr) {
    // parity: `MinecartTNT.explode`.
    const bool primed = cart.fuse > -1;
    if (!world.gameRules().get(world::gameRules::kTntExplodes)) {
        if (primed) {
            cart.removed = true;
        }
        return;
    }
    carts::CartState& state = carts::stateOf(cart);
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    const double speed = std::min(std::sqrt(speedSqr), carts::kTntCartSpeedCap);
    const double power = static_cast<double>(state.explosionPower) +
                         (static_cast<double>(state.explosionSpeedFactor) * random.nextDouble() * 1.5 * speed);
    cart.removed = true;
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    explosions_->blowUp(world, watchers, cart.x, cart.y, cart.z, static_cast<float>(power), "tnt", cart.entityId(),
                        primed ? world::explosions::Spares{world::explosions::sparesRails}
                               : world::explosions::Spares{});
    world::vibrations::emit(world.state(), "explode", cart.x, cart.y, cart.z, world.gameTime(),
                            {.entity = cart.uuid()});
}

bool VehicleSystem::fuel(world::LoadedWorld& world, PlayerSession& player, ServerEntity& cart, bool fuelItem) {
    static_cast<void>(world);
    // parity: `MinecartFurnace.interact`, fuel only from the tag and only while it fits.
    return fuelItem && carts::addFuel(cart, player.x, player.z);
}

void VehicleSystem::program(world::LoadedWorld& world, ServerEntity& cart, const std::string& command,
                            bool trackOutput) {
    static_cast<void>(world);
    if (!cart.sourceNbt.has_value()) {
        cart.sourceNbt = wire::nbt::NbtCompound{};
    }
    // parity: `handleSetCommandMinecart`.
    // it stops being tracked.
    cart.sourceNbt->putString("Command", command);
    cart.sourceNbt->putBoolean("TrackOutput", trackOutput);
    if (!trackOutput) {
        static_cast<void>(cart.sourceNbt->remove("LastOutput"));
    }
}

bool VehicleSystem::touchesFluid(world::LoadedWorld& world, double x, double y, double z, double width, double height,
                                 bool lava) {
    // parity: `updateFluidHeightAndDoFluidPushing`.
    // surface reaches its bottom.
    constexpr double kDeflate = 0.001;
    const double half = (width / 2.0) - kDeflate;
    const double minY = y + kDeflate;
    const double maxY = y + height - kDeflate;
    const world::FluidKind wanted = lava ? world::FluidKind::Lava : world::FluidKind::Water;
    for (std::int32_t bx = mth::floor(x - half); bx < static_cast<std::int32_t>(std::ceil(x + half)); ++bx) {
        for (std::int32_t by = mth::floor(minY); by < static_cast<std::int32_t>(std::ceil(maxY)); ++by) {
            for (std::int32_t bz = mth::floor(z - half); bz < static_cast<std::int32_t>(std::ceil(z + half)); ++bz) {
                const std::int32_t state = world.state().blockState(bx, by, bz).value_or(0);
                if (world::fluidFlow::kindOf(state) != wanted) {
                    continue;
                }
                const double top = by + static_cast<double>(world::fluidFlow::heightOf(
                                            state, world.state().blockState(bx, by + 1, bz).value_or(0)));
                if (top >= minY) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace basalt::server::runtime
