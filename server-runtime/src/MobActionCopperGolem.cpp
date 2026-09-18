#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ContainerOpeners.hpp>
#include <basalt/server/runtime/world/DoubleChests.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <array>
#include <cmath>
#include <string>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::protocol::play::SoundSource;
namespace golems = entity::copperGolems;

constexpr std::array<std::string_view, 4> kStatuePoses{"standing", "sitting", "running", "star"};
/// parity: `Direction.fromYRot`'s horizontal order.
constexpr std::array<std::string_view, 4> kFacings{"south", "west", "north", "east"};
/// parity: `COPPER_GOLEM`'s eye height, and `BehaviorUtils.throwItem`'s drop and push.
constexpr double kEyeHeight = 0.8125;
constexpr double kThrowDrop = 0.3;
constexpr double kThrowSpeed = 0.3;
/// parity: `Shearable.spawnAtLocation(level, stack, 1.5F)`.
constexpr double kShearDropHeight = 1.5;

[[nodiscard]] std::int32_t blockOf(double coordinate) { return static_cast<std::int32_t>(std::floor(coordinate)); }

} // namespace

void MobActionSystem::golemOpensChest(ServerEntity& golem, std::int32_t x, std::int32_t y, std::int32_t z, bool open) {
    world::LoadedWorld* world = worldOf(golem);
    if (world == nullptr || !hooks_.openersChanged) {
        return;
    }
    // parity: `CompoundContainer.startOpen`, which opens both halves of a double chest.
    const auto tell = [&](std::int32_t hx, std::int32_t hy, std::int32_t hz) {
        const std::int32_t openers = open ? world::containerOpeners::open(world->name(), hx, hy, hz)
                                          : world::containerOpeners::close(world->name(), hx, hy, hz);
        hooks_.openersChanged(*world, hx, hy, hz, openers);
    };
    if (const auto halves = world::doubleChests::halves(world->state(), x, y, z); halves.has_value()) {
        tell(halves->first[0], halves->first[1], halves->first[2]);
        tell(halves->second[0], halves->second[1], halves->second[2]);
        return;
    }
    tell(x, y, z);
}

bool MobActionSystem::chestInUse(const ServerEntity& mob, std::int32_t x, std::int32_t y, std::int32_t z) {
    world::LoadedWorld* world = worldOf(mob);
    return world != nullptr && world::containerOpeners::count(world->name(), x, y, z) > 0;
}

void MobActionSystem::mobSound(ServerEntity& mob, std::string_view sound) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    broadcaster_->sendEffects(
        watchersOf(*world), blockOf(mob.x), blockOf(mob.y), blockOf(mob.z), server::registry::blocks::air().stateId,
        {world::blockEffect::Sound{
            .name = std::string{sound}, .source = SoundSource::Neutral, .volume = 1.0F, .pitch = 1.0F}});
}

void MobActionSystem::heldItemChanged(ServerEntity& mob) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    using wire::protocol::play::SetEquipmentPacket;
    const std::optional<server::world::loot::ItemStack> held = golems::mainHand(mob);
    const server::registry::Item* item =
        held.has_value() ? server::registry::items::registry().byName(held->item) : nullptr;
    SetEquipmentPacket::Slot slot{.type = SetEquipmentPacket::SlotType::MainHand};
    if (item != nullptr) {
        slot.stack = wire::protocol::play::WireItemStack{.itemId = item->protocolId, .count = held->count};
    }
    for (PlayerSession* player : watchersOf(*world)) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(SetEquipmentPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                SetEquipmentPacket{.entityId = mob.entityId(), .slots = {slot}}.write(buffer);
            });
        }
    }
}

void MobActionSystem::containerChanged(ServerEntity& mob, std::int32_t x, std::int32_t y, std::int32_t z) {
    world::LoadedWorld* world = worldOf(mob);
    if (world != nullptr) {
        world::redstoneComponents::scheduleComparatorsAround(world->state(), world->name(), x, y, z);
    }
}

void MobActionSystem::tickCopperGolem(world::LoadedWorld& world, ServerEntity& golem, random::RandomSource& random) {
    golems::GolemState& state = golems::stateOf(golem, random);
    // parity: `CarvedPumpkinBlock.trySpawnGolem` ends in `spawn(weatherState)`: the copper the pattern
    // was built on, now the chest under the golem, and the spawn sound.
    if (state.fromPattern) {
        state.fromPattern = false;
        const std::optional<std::int32_t> below =
            world.state().blockState(blockOf(golem.x), blockOf(golem.y) - 1, blockOf(golem.z));
        const wire::common::Identifier* block =
            below.has_value() ? server::registry::blockStateDefinitions::blockOf(*below) : nullptr;
        if (block != nullptr) {
            state.weather = golems::weatherOfBlock(block->path());
        }
        mobSound(golem, "entity.copper_golem.spawn");
    }
    // parity: `CountDownCooldownTicks` in the core activity, before anything idle runs.
    if (state.transportCooldown > 0) {
        state.transportCooldown--;
    }
    // parity: `updateWeathering`, which runs even without AI, on the level's own random.
    if (state.nextWeatheringTick != golems::kWaxed) {
        const std::int64_t now = world.gameTime();
        if (state.nextWeatheringTick == golems::kUnset) {
            state.nextWeatheringTick =
                now + random.nextIntBetweenInclusive(golems::kWeatheringFrom, golems::kWeatheringTo);
        } else {
            const bool oxidized = state.weather == golems::Weather::Oxidized;
            if (now >= state.nextWeatheringTick && !oxidized) {
                state.weather = golems::next(state.weather);
                state.nextWeatheringTick =
                    state.weather == golems::Weather::Oxidized
                        ? 0
                        : state.nextWeatheringTick +
                              random.nextIntBetweenInclusive(golems::kWeatheringFrom, golems::kWeatheringTo);
            }
            if (oxidized) {
                const std::optional<std::int32_t> here =
                    world.state().blockState(blockOf(golem.x), blockOf(golem.y), blockOf(golem.z));
                if (here.has_value() && *here == server::registry::blocks::air().stateId &&
                    random.nextFloat() <= golems::kStatueChance) {
                    turnToStatue(world, golem, random);
                }
            }
        }
    }
    golems::save(golem);
}

void MobActionSystem::turnToStatue(world::LoadedWorld& world, ServerEntity& golem, random::RandomSource& random) {
    const std::int32_t x = blockOf(golem.x);
    const std::int32_t y = blockOf(golem.y);
    const std::int32_t z = blockOf(golem.z);
    // parity: the pose is the golem's own roll, the facing its body turned to a side.
    const std::string pose{kStatuePoses[static_cast<std::size_t>(random.nextInt(0, 4))]};
    const auto quarter = static_cast<std::int32_t>(std::floor((golem.yaw / 90.0F) + 0.5F)) & 3;
    const wire::common::Identifier statue = wire::common::Identifier::vanilla("oxidized_copper_golem_statue");
    const std::optional<std::int32_t> stateId = server::registry::blockStateDefinitions::stateIdOf(
        statue, {{"copper_golem_pose", pose}, {"facing", std::string{kFacings[static_cast<std::size_t>(quarter)]}}},
        std::nullopt);
    if (!stateId.has_value() || !world.state().setBlockState(x, y, z, *stateId)) {
        return;
    }
    wire::nbt::NbtCompound data;
    if (golem.customName.has_value()) {
        wire::nbt::NbtCompound components;
        components.putString("minecraft:custom_name", *golem.customName);
        data.set("components", wire::nbt::NbtTag::ofCompound(std::move(components)));
    }
    if (server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4); chunk != nullptr) {
        chunk->setBlockEntity(server::world::ChunkBlockEntity{
            x & 15, y, z & 15, wire::common::Identifier::vanilla("copper_golem_statue"), std::move(data)});
    }
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    broadcaster_->announceBlock(watchers, world::BlockChange{.x = x, .y = y, .z = z, .stateId = *stateId});
    broadcaster_->notifyAround(world, watchers, x, y, z, server::registry::blocks::air().stateId);
    // parity: `dropPreservedEquipment`, what it was carrying for good.
    if (const std::optional<server::world::loot::ItemStack> held = golems::mainHand(golem); held.has_value()) {
        world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                   .uuid = wire::common::Uuid::random(),
                                                   .item = held->item,
                                                   .count = held->count,
                                                   .x = golem.x,
                                                   .y = golem.y,
                                                   .z = golem.z,
                                                   .pickupDelay = world::kSpawnedPickupDelay,
                                                   .components = held->components});
    }
    mobSound(golem, "entity.copper_golem_become_statue");
    golem.removed = true;
}

MobActionSystem::GolemUse MobActionSystem::useOnCopperGolem(world::LoadedWorld& world, PlayerSession& player,
                                                            ServerEntity& golem,
                                                            const std::optional<wire::common::Identifier>& held) {
    random::RandomSource random = freshRandom();
    golems::GolemState& state = golems::stateOf(golem, random);
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    // parity: an empty hand takes back whatever the golem is carrying, thrown at the player.
    if (!held.has_value()) {
        const std::optional<server::world::loot::ItemStack> carried = golems::mainHand(golem);
        if (!carried.has_value()) {
            return GolemUse::Pass;
        }
        const double dx = player.x - golem.x;
        const double dy = player.y - golem.y;
        const double dz = player.z - golem.z;
        const double length = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
        const double scale = length < 1.0E-4 ? 0.0 : kThrowSpeed / length;
        world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                   .uuid = wire::common::Uuid::random(),
                                                   .item = carried->item,
                                                   .count = carried->count,
                                                   .x = golem.x,
                                                   .y = golem.y + kEyeHeight - kThrowDrop,
                                                   .z = golem.z,
                                                   .velocityX = dx * scale,
                                                   .velocityY = dy * scale,
                                                   .velocityZ = dz * scale,
                                                   .components = carried->components});
        golems::setMainHand(golem, std::nullopt, false);
        heldItemChanged(golem);
        return GolemUse::Done;
    }
    const std::string_view path = held->path();
    const auto at = std::array<std::int32_t, 3>{blockOf(golem.x), blockOf(golem.y), blockOf(golem.z)};
    const auto levelEvent = [&](std::int32_t event) {
        broadcaster_->sendEffects(watchers, at[0], at[1], at[2], server::registry::blocks::air().stateId,
                                  {world::blockEffect::Effect{.type = event}});
    };
    // parity: `shear`, the poppy on its head dropped a block and a half up.
    if (path == "shears") {
        const wire::nbt::NbtCompound* equipment =
            golem.sourceNbt.has_value() ? golem.sourceNbt->getCompound("equipment") : nullptr;
        const wire::nbt::NbtCompound* antenna = equipment == nullptr ? nullptr : equipment->getCompound("saddle");
        if (antenna == nullptr || antenna->getString("id") != "minecraft:poppy") {
            return GolemUse::Pass;
        }
        const std::optional<server::world::loot::ItemStack> dropped = world::blockInventory::readStack(*antenna);
        wire::nbt::NbtCompound kept = *equipment;
        static_cast<void>(kept.remove("saddle"));
        golem.sourceNbt->set("equipment", wire::nbt::NbtTag::ofCompound(std::move(kept)));
        mobSound(golem, "entity.copper_golem.shear");
        if (dropped.has_value()) {
            world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                       .uuid = wire::common::Uuid::random(),
                                                       .item = dropped->item,
                                                       .count = dropped->count,
                                                       .x = golem.x,
                                                       .y = golem.y + kShearDropHeight,
                                                       .z = golem.z,
                                                       .pickupDelay = world::kSpawnedPickupDelay});
        }
        return GolemUse::DamageTool;
    }
    // parity: `HONEYCOMB` waxes it, `WAX_ON` (3003).
    if (path == "honeycomb" && state.nextWeatheringTick != golems::kWaxed) {
        levelEvent(3003);
        state.nextWeatheringTick = golems::kWaxed;
        golems::save(golem);
        return GolemUse::ConsumeOne;
    }
    if (path.ends_with("_axe")) {
        // parity: an axe on a waxed golem scrapes the wax off, `WAX_OFF` (3004).
        if (state.nextWeatheringTick == golems::kWaxed) {
            mobSound(golem, "item.axe.scrape");
            levelEvent(3004);
            state.nextWeatheringTick = golems::kUnset;
            golems::save(golem);
            return GolemUse::DamageTool;
        }
        // parity: and on a weathered one scrapes a stage off, `SCRAPE` (3005).
        if (state.weather != golems::Weather::Unaffected) {
            mobSound(golem, "item.axe.scrape");
            levelEvent(3005);
            state.nextWeatheringTick = golems::kUnset;
            state.weather = golems::previous(state.weather);
            golems::save(golem);
            return GolemUse::DamageTool;
        }
    }
    return GolemUse::Pass;
}

bool MobActionSystem::awakenStatue(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    std::map<std::string, std::string> properties = server::registry::blockStateDefinitions::propertiesOf(*stateId);
    std::optional<std::string> name;
    if (const server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4); chunk != nullptr) {
        if (const server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
            entity != nullptr) {
            if (const wire::nbt::NbtCompound* components = entity->data().getCompound("components");
                components != nullptr && !components->getString("minecraft:custom_name").empty()) {
                name = std::string{components->getString("minecraft:custom_name")};
            }
        }
    }
    // parity: `Direction.toYRot`, the golem facing the way the statue did.
    const auto facing = std::ranges::find(kFacings, properties["facing"]);
    const float yaw =
        facing == kFacings.end() ? 0.0F : 90.0F * static_cast<float>(std::distance(kFacings.begin(), facing));
    ServerEntity* golem = spawning_->spawnMob(world, wire::common::Identifier::vanilla("copper_golem"), x + 0.5,
                                              static_cast<double>(y), z + 0.5, yaw, "triggered", {});
    if (golem == nullptr) {
        return false;
    }
    golem->headYaw = yaw;
    golem->customName = name;
    mobSound(*golem, "entity.copper_golem.spawn");
    // parity: `level.removeBlock(pos, false)`, air written with flag three.
    const std::int32_t air = server::registry::blocks::air().stateId;
    if (world.state().setBlockState(x, y, z, air)) {
        const std::vector<PlayerSession*> watchers = watchersOf(world);
        broadcaster_->announceBlock(watchers, world::BlockChange{.x = x, .y = y, .z = z, .stateId = air});
        broadcaster_->notifyAround(world, watchers, x, y, z, *stateId);
    }
    return true;
}

void MobActionSystem::golemStruckByLightning(ServerEntity& golem, std::int32_t boltId) {
    random::RandomSource random = freshRandom();
    golems::GolemState& state = golems::stateOf(golem, random);
    // parity: `CopperGolem.thunderHit`, one stage scraped off per bolt.
    if (state.lastLightning == boltId) {
        return;
    }
    state.lastLightning = boltId;
    if (state.weather != golems::Weather::Unaffected) {
        state.nextWeatheringTick = golems::kUnset;
        state.weather = golems::previous(state.weather);
        golems::save(golem);
    }
}

} // namespace basalt::server::runtime
