#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/MobStatusEffects.hpp>
#include <basalt/server/runtime/player/PlayerEntityData.hpp>
#include <basalt/server/runtime/player/PlayerImpulse.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::runtime::player {
namespace {

using wire::protocol::play::Pose;
using wire::protocol::play::SetEntityDataPacket;
using Entry = SetEntityDataPacket::Entry;

/// parity: `Parrot.Variant`'s five, which `byId` clamps into.
constexpr std::int32_t kParrotVariants = 5;

/// parity: `Avatar.POSES`, width and height.
struct Dimensions {
    double width;
    double height;
};

[[nodiscard]] Dimensions dimensionsOf(Pose pose) {
    switch (pose) {
    case Pose::Sleeping:
    case Pose::Dying:
        return {0.2, 0.2};
    case Pose::FallFlying:
    case Pose::Swimming:
    case Pose::SpinAttack:
        return {0.6, 0.6};
    case Pose::Crouching:
        return {0.6, 1.5};
    default:
        return {0.6, 1.8};
    }
}

/// parity: `Player.canPlayerFitWithinBlocksAndEntitiesWhen`, against the blocks.
[[nodiscard]] bool fits(const PlayerSession& session, Pose pose, const entity::entityCollision::BlockAt& blockAt) {
    const Dimensions size = dimensionsOf(pose);
    constexpr double kDeflate = 1.0e-7;
    return !entity::entityCollision::collides(
        blockAt, entity::Aabb::of(session.x, session.y, session.z, size.width, size.height).inflate(-kDeflate));
}

[[nodiscard]] bool waterAt(const entity::entityCollision::BlockAt& blockAt, double x, double y, double z) {
    const std::optional<std::int32_t> state =
        blockAt(static_cast<std::int32_t>(std::floor(x)), static_cast<std::int32_t>(std::floor(y)),
                static_cast<std::int32_t>(std::floor(z)));
    return state.has_value() && world::fluids::of(*state) == world::Submersion::Water;
}

/// parity: `Player.getDesiredPose`.
[[nodiscard]] Pose desiredPose(const PlayerSession& session) {
    if (session.sleepingBed.has_value()) {
        return Pose::Sleeping;
    }
    if (session.swimming) {
        return Pose::Swimming;
    }
    if (session.fallFlying) {
        return Pose::FallFlying;
    }
    return session.sneaking && !session.flying ? Pose::Crouching : Pose::Standing;
}

[[nodiscard]] SetEntityDataPacket::ParticlesValue particlesOf(const entity::mobStatusEffects::Swirl& swirl) {
    SetEntityDataPacket::ParticlesValue particles;
    for (const entity::mobStatusEffects::Swirl::Particle& particle : swirl.particles) {
        const server::registry::ParticleType* type = server::registry::particleTypes::of(particle.particle);
        if (type == nullptr) {
            continue;
        }
        particles.particles.push_back(SetEntityDataPacket::ParticleValue{
            .particleId = type->protocolId,
            .options = particle.argb.has_value() ? std::optional{wire::protocol::play::ParticleOptions{
                                                       wire::protocol::play::ParticleOptions::Color{*particle.argb}}}
                                                 : std::nullopt});
    }
    return particles;
}

} // namespace

std::optional<std::int32_t> parrotVariantOf(const std::optional<wire::nbt::NbtCompound>& tag) {
    if (!tag.has_value() || tag->getString("id") != "minecraft:parrot") {
        return std::nullopt;
    }
    return std::clamp(tag->getInt("Variant"), 0, kParrotVariants - 1);
}

PlayerEntityData PlayerEntityData::of(const PlayerSession& session) {
    PlayerEntityData data;
    const bool spectator = session.gamemode == wire::protocol::play::Gamemode::Spectator;
    if (session.fireTicks > 0) {
        data.sharedFlags |= SetEntityDataPacket::kFlagOnFire;
    }
    if (session.sneaking) {
        data.sharedFlags |= SetEntityDataPacket::kFlagSneaking;
    }
    if (session.sprinting) {
        data.sharedFlags |= SetEntityDataPacket::kFlagSprinting;
    }
    if (session.swimming) {
        data.sharedFlags |= SetEntityDataPacket::kFlagSwimming;
    }
    // parity: `ServerPlayer.updateInvisibilityStatus`: a spectator is invisible and swirls nothing.
    if (spectator || session.effects.contains(wire::common::Identifier::vanilla("invisibility"))) {
        data.sharedFlags |= SetEntityDataPacket::kFlagInvisible;
    }
    if (session.effects.contains(wire::common::Identifier::vanilla("glowing"))) {
        data.sharedFlags |= SetEntityDataPacket::kFlagGlowing;
    }
    if (session.fallFlying) {
        data.sharedFlags |= SetEntityDataPacket::kFlagFallFlying;
    }
    data.airSupply = session.airSupply;
    // parity: `LivingEntity.die` sets `DYING`, which the respawn replaces.
    data.pose = session.dead ? Pose::Dying : session.pose;
    if (session.usingItem) {
        data.livingFlags = kLivingFlagUsingItem | (session.usingOffhand ? kLivingFlagOffhand : 0);
    }
    data.health = session.health();
    if (!spectator && !session.effects.empty()) {
        const entity::mobStatusEffects::Swirl swirl = entity::mobStatusEffects::swirlOf(session.effects);
        data.effectParticles = particlesOf(swirl);
        data.effectsAmbient = swirl.allAmbient;
    }
    data.arrowCount = session.arrowCount;
    data.stingerCount = session.stingerCount;
    if (session.sleepingBed.has_value()) {
        const std::array<std::int32_t, 3>& bed = *session.sleepingBed;
        data.sleepingPos = wire::protocol::play::BlockPos{bed[0], bed[1], bed[2]};
    }
    data.mainArm = session.mainArm;
    data.skinParts = session.skinParts;
    data.absorption = session.absorption;
    data.score = session.experience.score();
    data.shoulderLeft = parrotVariantOf(session.shoulderLeft);
    data.shoulderRight = parrotVariantOf(session.shoulderRight);
    return data;
}

std::vector<Entry> PlayerEntityData::changesFrom(const PlayerEntityData* previous) const {
    static const PlayerEntityData kDefaults{};
    const PlayerEntityData& before = previous == nullptr ? kDefaults : *previous;
    std::vector<Entry> entries;
    const auto add = [&entries](std::uint8_t index, SetEntityDataPacket::Value value) {
        entries.push_back(Entry{.index = index, .value = std::move(value)});
    };
    if (sharedFlags != before.sharedFlags) {
        add(SetEntityDataPacket::kSharedFlagsIndex,
            SetEntityDataPacket::ByteValue{.value = static_cast<std::int8_t>(sharedFlags)});
    }
    if (airSupply != before.airSupply) {
        add(SetEntityDataPacket::kAirSupplyIndex, SetEntityDataPacket::IntValue{.value = airSupply});
    }
    if (pose != before.pose) {
        add(SetEntityDataPacket::kPoseIndex, SetEntityDataPacket::PoseValue{.pose = pose});
    }
    if (ticksFrozen != before.ticksFrozen) {
        add(kTicksFrozenIndex, SetEntityDataPacket::IntValue{.value = ticksFrozen});
    }
    if (livingFlags != before.livingFlags) {
        add(SetEntityDataPacket::kLivingFlagsIndex,
            SetEntityDataPacket::ByteValue{.value = static_cast<std::int8_t>(livingFlags)});
    }
    if (health != before.health) {
        add(kHealthIndex, SetEntityDataPacket::FloatValue{.value = health});
    }
    if (effectParticles != before.effectParticles) {
        add(kEffectParticlesIndex, effectParticles);
    }
    if (effectsAmbient != before.effectsAmbient) {
        add(kEffectAmbienceIndex, SetEntityDataPacket::BoolValue{.value = effectsAmbient});
    }
    if (arrowCount != before.arrowCount) {
        add(kArrowCountIndex, SetEntityDataPacket::IntValue{.value = arrowCount});
    }
    if (stingerCount != before.stingerCount) {
        add(kStingerCountIndex, SetEntityDataPacket::IntValue{.value = stingerCount});
    }
    if (sleepingPos != before.sleepingPos) {
        add(SetEntityDataPacket::kSleepingPosIndex, SetEntityDataPacket::OptionalBlockPosValue{.pos = sleepingPos});
    }
    if (mainArm != before.mainArm) {
        add(kMainHandIndex, SetEntityDataPacket::RegistryIdValue{
                                .value = mainArm, .registry = wire::protocol::play::RegistryHolder::HumanoidArm});
    }
    if (skinParts != before.skinParts) {
        add(SetEntityDataPacket::kPlayerSkinPartsIndex, SetEntityDataPacket::ByteValue{.value = skinParts});
    }
    if (absorption != before.absorption) {
        add(SetEntityDataPacket::kPlayerAbsorptionIndex, SetEntityDataPacket::FloatValue{.value = absorption});
    }
    if (score != before.score) {
        add(kScoreIndex, SetEntityDataPacket::IntValue{.value = score});
    }
    if (shoulderLeft != before.shoulderLeft) {
        add(kShoulderLeftIndex, SetEntityDataPacket::OptionalUnsignedIntValue{.value = shoulderLeft});
    }
    if (shoulderRight != before.shoulderRight) {
        add(kShoulderRightIndex, SetEntityDataPacket::OptionalUnsignedIntValue{.value = shoulderRight});
    }
    return entries;
}

void tickBody(PlayerSession& session, const entity::entityCollision::BlockAt& blockAt) {
    impulse::tick(session);
    // parity: `LivingEntity.tick`, one arrow or stinger falling out at a time.
    if (session.arrowCount > 0) {
        if (session.removeArrowTime <= 0) {
            session.removeArrowTime = 20 * (30 - session.arrowCount);
        }
        if (--session.removeArrowTime <= 0) {
            --session.arrowCount;
        }
    }
    if (session.stingerCount > 0) {
        if (session.removeStingerTime <= 0) {
            session.removeStingerTime = 20 * (30 - session.stingerCount);
        }
        if (--session.removeStingerTime <= 0) {
            --session.stingerCount;
        }
    }
    // parity: `Player.updateSwimming`: a flier never swims; otherwise it takes the eyes under water
    // and water at the feet to start, and only being in water to go on.
    const bool riding = session.vehicleId.has_value();
    if (session.flying) {
        session.swimming = false;
    } else {
        const double eyeY = session.y + (session.pose == Pose::Swimming || session.pose == Pose::FallFlying
                                             ? 0.4
                                             : world::fluids::eyeHeight(session.pose == Pose::Crouching));
        const bool feetInWater = waterAt(blockAt, session.x, session.y, session.z);
        if (session.swimming) {
            session.swimming =
                session.sprinting && !riding && (feetInWater || waterAt(blockAt, session.x, eyeY, session.z));
        } else {
            session.swimming =
                session.sprinting && !riding && waterAt(blockAt, session.x, eyeY, session.z) && feetInWater;
        }
    }
    // parity: `Player.updatePlayerPose`.
    if (!fits(session, Pose::Swimming, blockAt)) {
        return;
    }
    const Pose desired = desiredPose(session);
    const bool spectator = session.gamemode == wire::protocol::play::Gamemode::Spectator;
    if (spectator || riding || fits(session, desired, blockAt)) {
        session.pose = desired;
    } else {
        session.pose = fits(session, Pose::Crouching, blockAt) ? Pose::Crouching : Pose::Swimming;
    }
}

} // namespace basalt::server::runtime::player
