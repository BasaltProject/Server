#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/EnderEyes.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/world/terrain/StrongholdStructureGenerator.hpp>

#include <cmath>
#include <memory>
#include <string>

namespace basalt::server::runtime::world::enderEyes {
namespace {

using entity::ServerEntity;
using entity::enderEyes::EyeState;
using wire::common::Identifier;
using wire::protocol::play::Gamemode;
using wire::protocol::play::Pose;

constexpr std::string_view kEnderEye = "ender_eye";
/// parity: `EyeOfEnder.TOO_FAR_DISTANCE` and `TOO_FAR_SIGNAL_HEIGHT`.
constexpr double kTooFar = 12.0;
constexpr double kTooFarHeight = 8.0;
/// parity: the eighty ticks an eye flies before it drops or breaks.
constexpr std::int32_t kLife = 80;
/// parity: `LevelEvent.PARTICLES_EYE_OF_ENDER_DEATH`.
constexpr std::int32_t kShatterEvent = 2003;
/// parity: `Player.blockInteractionRange`, 4.5 and half a block more in creative.
constexpr double kReach = 4.5;
constexpr double kCreativeReach = 5.0;
constexpr double kPiOver180 = 0.017453292519943295;

/// parity: `Entity.getBbHeight` and `getEyeHeight` of a player in each pose.
[[nodiscard]] double heightOf(Pose pose) {
    switch (pose) {
    case Pose::Crouching:
        return 1.5;
    case Pose::Swimming:
    case Pose::FallFlying:
    case Pose::SpinAttack:
        return 0.6;
    case Pose::Sleeping:
    case Pose::Dying:
        return 0.2;
    default:
        return 1.8;
    }
}

[[nodiscard]] double eyeHeightOf(Pose pose) {
    switch (pose) {
    case Pose::Crouching:
        return 1.27;
    case Pose::Swimming:
    case Pose::FallFlying:
    case Pose::SpinAttack:
        return 0.4;
    case Pose::Sleeping:
    case Pose::Dying:
        return 0.2;
    default:
        return 1.62;
    }
}

/// parity: `getPlayerPOVHitResult(..., Fluid.NONE)` landing on an end portal frame, which leaves the
/// use to `useOn`. The frame's collision box is its outline, so the collider clip answers the same.
[[nodiscard]] bool lookingAtFrame(WorldState& state, const player::PlayerSession& player) {
    const double reach = player.gamemode == Gamemode::Creative ? kCreativeReach : kReach;
    const double yaw = static_cast<double>(player.yaw) * kPiOver180;
    const double pitch = static_cast<double>(player.pitch) * kPiOver180;
    const double fromX = player.x;
    const double fromY = player.y + eyeHeightOf(player.pose);
    const double fromZ = player.z;
    const std::optional<entity::BlockHit> hit =
        entity::blockRay::cast(state, fromX, fromY, fromZ, fromX + (-std::sin(yaw) * std::cos(pitch) * reach),
                               fromY + (-std::sin(pitch) * reach), fromZ + (std::cos(yaw) * std::cos(pitch) * reach));
    if (!hit.has_value()) {
        return false;
    }
    const std::optional<std::int32_t> block = state.blockState(hit->blockX, hit->blockY, hit->blockZ);
    const Identifier* name = block.has_value() ? server::registry::blockStateDefinitions::blockOf(*block) : nullptr;
    return name != nullptr && name->path() == "end_portal_frame";
}

void spendOne(const player::BreakContext& context, player::PlayerSession& player, std::int32_t slot) {
    const std::optional<storage::StoredItemStack>& stack = player.inventory.at(slot);
    if (!stack.has_value()) {
        return;
    }
    if (stack->count <= 1) {
        player.inventory.set(slot, std::nullopt);
    } else {
        storage::StoredItemStack left = *stack;
        left.count = stack->count - 1;
        player.inventory.set(slot, left);
    }
    if (context.slotChanged) {
        context.slotChanged(player, slot);
    }
}

[[nodiscard]] EyeState& stateOf(ServerEntity& eye) {
    if (eye.enderEye == nullptr) {
        eye.enderEye = std::make_shared<EyeState>();
    }
    return *eye.enderEye;
}

} // namespace

const Identifier& type() {
    static const Identifier kType = Identifier::vanilla("eye_of_ender");
    return kType;
}

bool isEye(const ServerEntity& entity) { return entity.type() == type(); }

void signalTo(ServerEntity& eye, double x, double y, double z, random::RandomSource& random) {
    const double dx = x - eye.x;
    const double dz = z - eye.z;
    const double horizontal = std::sqrt((dx * dx) + (dz * dz));
    EyeState& state = stateOf(eye);
    if (horizontal > kTooFar) {
        state.target =
            std::array{eye.x + (dx / horizontal * kTooFar), eye.y + kTooFarHeight, eye.z + (dz / horizontal * kTooFar)};
    } else {
        state.target = std::array{x, y, z};
    }
    state.life = 0;
    state.surviveAfterDeath = random.nextInt(5) > 0;
}

Fate step(ServerEntity& eye) {
    EyeState& state = stateOf(eye);
    const double newX = eye.x + eye.velocityX;
    const double newY = eye.y + eye.velocityY;
    const double newZ = eye.z + eye.velocityZ;
    if (state.target.has_value()) {
        // parity: `updateDeltaMovement`, NaN and all when it sits exactly over its target.
        const auto& [targetX, targetY, targetZ] = *state.target;
        const double deltaX = targetX - newX;
        const double deltaZ = targetZ - newZ;
        const double length = std::sqrt((deltaX * deltaX) + (deltaZ * deltaZ));
        const double oldHorizontal = std::sqrt((eye.velocityX * eye.velocityX) + (eye.velocityZ * eye.velocityZ));
        double wantedSpeed = oldHorizontal + (0.0025 * (length - oldHorizontal));
        double movementY = eye.velocityY;
        if (length < 1.0) {
            wantedSpeed *= 0.8;
            movementY *= 0.8;
        }
        const double wantedY = newY - eye.velocityY < targetY ? 1.0 : -1.0;
        const double scale = wantedSpeed / length;
        eye.velocityX = deltaX * scale;
        eye.velocityY = movementY + ((wantedY - movementY) * 0.015);
        eye.velocityZ = deltaZ * scale;
    }
    eye.x = newX;
    eye.y = newY;
    eye.z = newZ;
    ++state.life;
    if (state.life <= kLife) {
        return Fate::Flying;
    }
    eye.removed = true;
    return state.surviveAfterDeath ? Fate::Dropped : Fate::Shattered;
}

bool throwFor(const player::BreakContext& context, player::PlayerSession& player, LoadedWorld& world,
              std::int32_t slot) {
    const std::optional<storage::StoredItemStack> held = player.inventory.at(slot);
    if (!held.has_value()) {
        return false;
    }
    const auto item = Identifier::parse(held->id);
    if (!item.has_value() || item->path() != kEnderEye) {
        return false;
    }
    if (lookingAtFrame(world.state(), player)) {
        return false;
    }
    // parity: `#eye_of_ender_located` is the stronghold alone, whose rings are all it asks.
    server::world::terrain::StrongholdStructureGenerator* strongholds = world.strongholdGenerator();
    const auto found = strongholds == nullptr ? std::nullopt
                                              : strongholds->locate(static_cast<std::int32_t>(std::floor(player.x)),
                                                                    static_cast<std::int32_t>(std::floor(player.z)));
    const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(type());
    if (!found.has_value() || entry == nullptr || context.registry == nullptr) {
        return true;
    }
    // parity: `StructurePlacement.getLocatePos`, the chunk's lowest corner at height zero.
    const double featureX = static_cast<double>(found->first) * 16.0;
    const double featureZ = static_cast<double>(found->second) * 16.0;

    auto made =
        std::make_unique<ServerEntity>(context.registry->allocateEntityId(), wire::common::Uuid::random(), type(),
                                       entry->protocolId, player.x, player.y + (heightOf(player.pose) * 0.5), player.z);
    wire::nbt::NbtCompound stack;
    stack.putString("id", item->toString());
    stack.putInt("count", 1);
    if (!held->components.empty()) {
        stack.set("components", wire::nbt::NbtTag::ofCompound(held->components));
    }
    wire::nbt::NbtCompound data;
    data.set("Item", wire::nbt::NbtTag::ofCompound(std::move(stack)));
    made->sourceNbt = std::move(data);
    random::RandomSource own = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    signalTo(*made, featureX, 0.0, featureZ, own);
    ServerEntity& eye = world.state().spawnEntity(std::move(made));
    vibrations::emit(world.state(), "projectile_shoot", eye.x, eye.y, eye.z, world.gameTime(),
                     {.entity = player.uuid(), .steppingCarefully = player.sneaking});

    if (context.triggers != nullptr) {
        const double dx = player.x - featureX;
        const double dz = player.z - featureZ;
        // parity: `UsedEnderEyeTrigger`, which hands over the squared horizontal distance.
        context.triggers->fire("used_ender_eye", player.name(), player.uuid().toString(),
                               TriggerEvent{.numbers = {{"distance", (dx * dx) + (dz * dz)}}});
    }
    random::RandomSource level = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    const float pitch = 0.33F + (level.nextFloat() * (0.5F - 0.33F));
    const std::int64_t seed = context.nextSeed ? context.nextSeed() : 0;
    for (const player::PlayerSession* heard : player::playersIn(context, world.name())) {
        player::playSoundFor(*heard, "entity.ender_eye.launch", wire::protocol::play::SoundSource::Neutral, player.x,
                             player.y, player.z, 1.0F, pitch, seed);
    }
    if (player.gamemode != Gamemode::Creative) {
        spendOne(context, player, slot);
    }
    player::statAwards::used(player, item->toString());
    return true;
}

void tick(LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, player::PlayerRegistry& registry,
          ServerEntity& eye, std::int64_t seed) {
    const Fate fate = step(eye);
    if (fate == Fate::Flying) {
        return;
    }
    for (const player::PlayerSession* heard : watchers) {
        player::playSoundFor(*heard, "entity.ender_eye.death", wire::protocol::play::SoundSource::Neutral, eye.x, eye.y,
                             eye.z, 1.0F, 1.0F, seed);
    }
    if (fate == Fate::Dropped) {
        const wire::nbt::NbtCompound* stack = eye.sourceNbt.has_value() ? eye.sourceNbt->getCompound("Item") : nullptr;
        const auto id = stack == nullptr ? Identifier::vanilla(std::string{kEnderEye})
                                         : Identifier::parse(std::string{stack->getString("id")})
                                               .value_or(Identifier::vanilla(std::string{kEnderEye}));
        const wire::nbt::NbtCompound* components = stack == nullptr ? nullptr : stack->getCompound("components");
        // parity: `new ItemEntity(level, x, y, z, stack)`, its hop drawn from the level's random.
        random::RandomSource random = random::RandomSource::legacy(seed);
        const double velocityX = (random.nextDouble() * 0.2) - 0.1;
        const double velocityZ = (random.nextDouble() * 0.2) - 0.1;
        world.state().spawnItem(
            DroppedItem{.entityId = registry.allocateEntityId(),
                        .uuid = wire::common::Uuid::random(),
                        .item = id,
                        .count = 1,
                        .x = eye.x,
                        .y = eye.y,
                        .z = eye.z,
                        .velocityX = velocityX,
                        .velocityY = 0.2,
                        .velocityZ = velocityZ,
                        .pickupDelay = 0,
                        .components = components == nullptr ? wire::nbt::NbtCompound{} : *components});
        return;
    }
    const auto x = static_cast<std::int32_t>(std::floor(eye.x));
    const auto y = static_cast<std::int32_t>(std::floor(eye.y));
    const auto z = static_cast<std::int32_t>(std::floor(eye.z));
    for (player::PlayerSession* watcher : watchers) {
        if (watcher->connection().sendLevelEvent) {
            watcher->connection().sendLevelEvent(kShatterEvent, x, y, z, 0);
        }
    }
}

} // namespace basalt::server::runtime::world::enderEyes
