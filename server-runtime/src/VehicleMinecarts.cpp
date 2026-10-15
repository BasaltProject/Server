#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/EntityPushSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/VehicleSystem.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/Minecarts.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/world/FallLanding.hpp>
#include <basalt/server/runtime/world/FluidCurrents.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/StuckSpeed.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

namespace basalt::server::runtime {
namespace {

using entity::Aabb;
using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;
namespace mth = wire::common::mth;
namespace carts = entity::minecarts;

constexpr double kCartWidth = static_cast<double>(0.98F);
constexpr double kCartHeight = static_cast<double>(0.7F);
/// parity: `MinecartCommandBlock.ACTIVATION_DELAY`.
constexpr std::int32_t kActivationDelay = 4;
/// parity: `Minecart.activateMinecart`'s shake.
constexpr float kActivatorDamage = 50.0F;
/// parity: `igniteForSeconds(15.0F)` in lava.
constexpr std::int32_t kLavaFireTicks = 300;

[[nodiscard]] Aabb boxOf(const ServerEntity& cart) { return Aabb::of(cart.x, cart.y, cart.z, kCartWidth, kCartHeight); }

[[nodiscard]] std::string_view pathAt(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    const Identifier* block = state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
    return block == nullptr ? std::string_view{} : std::string_view{block->path()};
}

} // namespace

void VehicleSystem::tickMinecart(world::LoadedWorld& world, ServerEntity& cart) {
    carts::CartState& state = carts::stateOf(cart);
    const bool experimental = options_.minecartImprovements && options_.minecartImprovements();
    state.experimental = experimental;
    ++cart.tickCount;
    if (state.lerpStepsTick != world.gameTime()) {
        state.lerpSteps.clear();
        state.lerpStepsTick = world.gameTime();
    }
    // parity: `AbstractMinecart.tick`: the shake and the damage wear off a tick at a time.
    if (cart.vehicleHurtTime > 0) {
        --cart.vehicleHurtTime;
    }
    if (cart.vehicleDamage > 0.0F) {
        cart.vehicleDamage -= 1.0F;
    }
    // parity: `checkBelowWorld`, sixty-four blocks under the floor of the world.
    const std::int32_t minY = config::dimensionFacts(world.declaration().dimension).chunkHeight.minY();
    if (cart.y < static_cast<double>(minY - 64)) {
        cart.removed = true;
        return;
    }
    carts::Env env;
    env.experimental = experimental;
    env.maxMinecartSpeed = world.gameRules().get(world::gameRules::kMaxMinecartSpeed);
    if (!cart.passengerIds.empty()) {
        for (const PlayerSession* player : watchersOf(world)) {
            if (player->entityId() == cart.passengerIds.front()) {
                env.riderIntent = carts::intentOf(player->inputForward, player->inputBack, player->inputLeft,
                                                  player->inputRight, player->yaw);
            }
        }
    }
    env.activate = [this, &world](ServerEntity& self, const std::array<std::int32_t, 3>& rail, bool powered) {
        activate(world, self, rail, powered);
    };
    env.pushAndPickup = [this, &world, experimental](ServerEntity& self) {
        return pushAndPickup(world, self, experimental);
    };
    env.insideBlocks = [this, &world](ServerEntity& self) { insideBlocks(world, self); };
    env.landed = [this, &world](ServerEntity& self, double fallDistance) { landed(world, self, fallDistance); };
    carts::tick(world.state(), cart, env);
    // parity: `updateFluidInteraction`, after the move, for next turn's speeds.
    const world::fluidCurrents::Contact contact = world::fluidCurrents::carry(world, cart, options_.blockTags);
    state.inWater = contact.inWater();
    state.inLava = contact.inLava();
    if (state.inLava && !cart.removed) {
        cart.fireTicks = std::max(cart.fireTicks, kLavaFireTicks);
        // parity: `lavaHurt`, four points and the burn sound when it lands.
        if (hurt(world, cart, Hit{.amount = 4.0F, .type = world::damageTypes::kLava}) &&
            !(cart.sourceNbt.has_value() && cart.sourceNbt->getBoolean("Silent"))) {
            random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
            const float pitch = 2.0F + (random.nextFloat() * 0.4F);
            for (const PlayerSession* player : watchersOf(world)) {
                player::playSoundFor(*player, "minecraft:entity.generic.burn",
                                     wire::protocol::play::SoundSource::Neutral, cart.x, cart.y, cart.z, 0.4F, pitch,
                                     random.nextLong());
            }
        }
        cart.fallDistance *= 0.5;
    }
    state.firstTick = false;
    if (cart.removed) {
        return;
    }
    const std::string_view path = cart.type().path();
    // parity: `MinecartFurnace.tick`: the fuel burns down, and with it goes the push.
    if (path == "furnace_minecart") {
        if (state.fuel > 0) {
            --state.fuel;
        }
        if (state.fuel <= 0) {
            state.pushX = 0.0;
            state.pushZ = 0.0;
        }
    }
    // parity: `MinecartTNT.tick`.
    // sets it off.
    if (path == "tnt_minecart") {
        const double speedSqr = (cart.velocityX * cart.velocityX) + (cart.velocityZ * cart.velocityZ);
        if (cart.fuse > 0) {
            --cart.fuse;
        } else if (cart.fuse == 0) {
            explodeTnt(world, cart, speedSqr);
        }
        if (!cart.removed && state.horizontalCollision && speedSqr >= static_cast<double>(0.01F)) {
            explodeTnt(world, cart, speedSqr);
        }
    }
    // parity: `MinecartSpawner.tick`, the spawner's own turn at the cart's square.
    if (path == "spawner_minecart" && options_.tickSpawner) {
        if (!cart.sourceNbt.has_value()) {
            cart.sourceNbt = wire::nbt::NbtCompound{};
        }
        static_cast<void>(options_.tickSpawner(world, *cart.sourceNbt,
                                               {mth::floor(cart.x), mth::floor(cart.y), mth::floor(cart.z)},
                                               [this, &world, &cart](std::int32_t eventId) {
                                                   entityEvent(world, cart, static_cast<std::int8_t>(eventId));
                                               }));
    }
}

void VehicleSystem::activate(world::LoadedWorld& world, ServerEntity& cart, const std::array<std::int32_t, 3>& rail,
                             bool powered) {
    static_cast<void>(rail);
    const std::string_view path = cart.type().path();
    if (path == "hopper_minecart") {
        // parity: `MinecartHopper.activateMinecart`, off while the rail is powered.
        if (!cart.sourceNbt.has_value()) {
            cart.sourceNbt = wire::nbt::NbtCompound{};
        }
        cart.sourceNbt->putBoolean("Enabled", !powered);
        return;
    }
    if (path == "tnt_minecart") {
        if (powered && cart.fuse < 0) {
            primeTnt(world, cart, std::nullopt);
        }
        return;
    }
    if (path == "minecart") {
        // parity: `Minecart.activateMinecart`.
        if (powered) {
            if (!cart.passengerIds.empty()) {
                ejectPassengers(world, cart);
            }
            if (cart.vehicleHurtTime == 0) {
                cart.vehicleHurtDir = -cart.vehicleHurtDir;
                cart.vehicleHurtTime = 10;
                cart.vehicleDamage = kActivatorDamage;
            }
        }
        return;
    }
    if (path != "command_block_minecart" || !powered) {
        return;
    }
    carts::CartState& state = carts::stateOf(cart);
    if (cart.tickCount - state.lastActivated < kActivationDelay) {
        return;
    }
    state.lastActivated = cart.tickCount;
    // parity: `BaseCommandBlock.performCommand` over the cart's own copy of the fields.
    if (!cart.sourceNbt.has_value()) {
        cart.sourceNbt = wire::nbt::NbtCompound{};
    }
    wire::nbt::NbtCompound& data = *cart.sourceNbt;
    if (data.getLong("LastExecution", -1) == world.gameTime()) {
        return;
    }
    const std::string command{data.getString("Command")};
    std::string lowered = command;
    std::ranges::transform(lowered, lowered.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowered == "searge") {
        data.putInt("SuccessCount", 1);
        return;
    }
    data.putInt("SuccessCount", 0);
    if (world.gameRules().get(world::gameRules::kCommandBlocksWork) && !command.empty() && options_.runCommand) {
        data.putInt("SuccessCount", std::max(0, options_.runCommand(world, cart, command)));
    }
    if (data.getBoolean("UpdateLastExecution", true)) {
        data.putLong("LastExecution", world.gameTime());
    } else {
        static_cast<void>(data.remove("LastExecution"));
    }
}

bool VehicleSystem::pushAndPickup(world::LoadedWorld& world, ServerEntity& cart, bool experimental) {
    const EntityPushSystem::Body self{.mob = &cart};
    const Aabb box = boxOf(cart);
    const Aabb reach{box.minX - 0.2, box.minY, box.minZ - 0.2, box.maxX + 0.2, box.maxY, box.maxZ + 0.2};
    const bool rideable = carts::isRideable(cart);
    const auto passenger = [](const EntityPushSystem::Body& body) {
        return body.mob != nullptr ? body.mob->vehicleId.has_value() : body.player->vehicleId.has_value();
    };
    // parity: whoever a moving empty cart takes aboard: no player, golem or cart, nobody riding.
    const auto takes = [&cart, &passenger](const EntityPushSystem::Body& body) {
        return body.mob != nullptr && body.mob->type().path() != "iron_golem" && !carts::isMinecart(body.mob->type()) &&
               cart.passengerIds.empty() && !passenger(body);
    };
    if (!experimental) {
        // parity: `OldMinecartBehavior.pushAndPickupEntities`.
        if (rideable && (cart.velocityX * cart.velocityX) + (cart.velocityZ * cart.velocityZ) >= 0.01) {
            for (const EntityPushSystem::Body& other : pushes_->pushablesIn(world, self, reach)) {
                if (takes(other) && registry::entityAttributes::isLiving(other.mob->type())) {
                    static_cast<void>(board(world, cart, *other.mob));
                } else {
                    pushes_->push(world, other, self);
                }
            }
        } else {
            for (const EntityPushSystem::Body& other : pushes_->entitiesIn(world, self, reach)) {
                if (other.mob != nullptr && carts::isMinecart(other.mob->type()) &&
                    std::ranges::find(cart.passengerIds, other.mob->entityId()) == cart.passengerIds.end() &&
                    pushes_->isPushable(world, other)) {
                    pushes_->push(world, other, self);
                }
            }
        }
        return false;
    }
    // parity: `NewMinecartBehavior.pushAndPickupEntities`.
    bool pickedUp = false;
    if (rideable && cart.passengerIds.empty()) {
        for (const EntityPushSystem::Body& other : pushes_->pushablesIn(world, self, reach)) {
            if (takes(other) && registry::entityAttributes::isLiving(other.mob->type()) &&
                board(world, cart, *other.mob)) {
                pickedUp = true;
                break;
            }
        }
    }
    carts::CartState& state = carts::stateOf(cart);
    if (!state.horizontalCollision && !state.verticalCollision) {
        return false;
    }
    const Aabb touching = box.inflate(1.0E-7);
    bool pushed = false;
    if (rideable) {
        for (const EntityPushSystem::Body& other : pushes_->pushablesIn(world, self, touching)) {
            const bool golemOrCart = other.mob != nullptr &&
                                     (other.mob->type().path() == "iron_golem" || carts::isMinecart(other.mob->type()));
            if (other.player != nullptr || golemOrCart || !cart.passengerIds.empty() || passenger(other)) {
                pushes_->push(world, other, self);
                pushed = true;
            }
        }
    } else {
        for (const EntityPushSystem::Body& other : pushes_->entitiesIn(world, self, touching)) {
            if (other.mob != nullptr && carts::isMinecart(other.mob->type()) &&
                std::ranges::find(cart.passengerIds, other.mob->entityId()) == cart.passengerIds.end() &&
                pushes_->isPushable(world, other)) {
                pushes_->push(world, other, self);
                pushed = true;
            }
        }
    }
    return pickedUp && !pushed;
}

void VehicleSystem::insideBlocks(world::LoadedWorld& world, ServerEntity& cart) {
    // parity: `Entity.applyEffectsFromBlocks` over the box shrunk by a hundred-thousandth.
    // blocks that do something to anything, not only to the living.
    const Aabb box = boxOf(cart).inflate(-static_cast<double>(1.0E-5F));
    bool fire = false;
    std::optional<float> fireDamage;
    for (std::int32_t x = mth::floor(box.minX); x <= mth::floor(box.maxX); ++x) {
        for (std::int32_t y = mth::floor(box.minY); y <= mth::floor(box.maxY); ++y) {
            for (std::int32_t z = mth::floor(box.minZ); z <= mth::floor(box.maxZ); ++z) {
                const std::string_view path = pathAt(world, x, y, z);
                if (path == "bubble_column") {
                    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
                    const std::map<std::string, std::string> properties =
                        registry::blockStateDefinitions::propertiesOf(stateId.value_or(0));
                    const bool down = properties.contains("drag") && properties.at("drag") == "true";
                    const std::int32_t above = world.state().blockState(x, y + 1, z).value_or(0);
                    const bool open =
                        registry::blockShapes::isEmptyShape(above) && !registry::blockFluids::hasFluid(above);
                    if (open) {
                        cart.velocityY =
                            down ? std::max(-0.9, cart.velocityY - 0.03) : std::min(1.8, cart.velocityY + 0.1);
                    } else {
                        cart.velocityY =
                            down ? std::max(-0.3, cart.velocityY - 0.03) : std::min(0.7, cart.velocityY + 0.06);
                    }
                } else if (path == "cactus") {
                    static_cast<void>(hurt(world, cart, Hit{.amount = 1.0F, .type = Identifier::vanilla("cactus")}));
                } else if (path == "fire" || path == "soul_fire") {
                    fire = true;
                    fireDamage = std::max(fireDamage.value_or(0.0F), path == "soul_fire" ? 2.0F : 1.0F);
                }
                if (cart.removed) {
                    return;
                }
            }
        }
    }
    // parity: `BaseFireBlock.entityInside`'s `FIRE_IGNITE` then the burn.
    if (fire) {
        cart.fireTicks = std::max(cart.fireTicks, 160);
        static_cast<void>(hurt(world, cart, Hit{.amount = *fireDamage, .type = Identifier::vanilla("in_fire")}));
    }
    if (cart.removed) {
        return;
    }
    const entity::entityCollision::BlockAt blockAt = [&world](std::int32_t x, std::int32_t y, std::int32_t z) {
        return world.state().blockState(x, y, z);
    };
    if (const std::optional<world::stuckSpeed::Multiplier> caught =
            world::stuckSpeed::caughtBy(blockAt, boxOf(cart), 0.0, 0.0, 0.0);
        caught.has_value()) {
        carts::stateOf(cart).stuck = {caught->x, caught->y, caught->z};
        cart.fallDistance = 0.0;
    }
}

void VehicleSystem::landed(world::LoadedWorld& world, ServerEntity& cart, double fallDistance) {
    // parity: `Block.fallOn` with the block a fifth under the feet, and its `HIT_GROUND`.
    const std::int32_t under =
        world.state().blockState(mth::floor(cart.x), mth::floor(cart.y - 0.2), mth::floor(cart.z)).value_or(0);
    const world::fallLanding::Landing landing = world::fallLanding::of(under, false);
    world::vibrations::emit(world.state(), "hit_ground", cart.x, cart.y, cart.z, world.gameTime(),
                            {.entity = cart.uuid()});
    const double distance = (fallDistance * landing.distanceScale) + landing.extraDistance;
    // parity: `MinecartTNT.causeFallDamage`, three blocks and it goes off.
    if (cart.type().path() == "tnt_minecart" && distance >= 3.0) {
        const double power = distance / 10.0;
        explodeTnt(world, cart, power * power);
    }
    // parity: `Entity.causeFallDamage` hands the fall to whoever rides it.
    const bool immune =
        options_.entityTags != nullptr &&
        world::realEntityTypeTags::membersOf(*options_.entityTags, Identifier::vanilla("fall_damage_immune"))
            .contains(cart.type());
    if (!immune && !cart.passengerIds.empty()) {
        mobs_->propagateFall(world, cart, fallDistance, landing);
    }
}

} // namespace basalt::server::runtime
