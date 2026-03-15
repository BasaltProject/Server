#pragma once

#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// Metadata needed to render immutable entities emitted by structure templates.
[[nodiscard]] std::vector<wire::protocol::play::SetEntityDataPacket::Entry>
generatedEntityMetadata(std::string_view type, const wire::nbt::NbtCompound& data);

/// One piece of gear a generated entity was written wearing.
struct GeneratedEquipment {
    wire::protocol::play::SetEquipmentPacket::SlotType slot =
        wire::protocol::play::SetEquipmentPacket::SlotType::MainHand;
    std::string item;
    std::int32_t count = 0;
    wire::nbt::NbtCompound components;
};

[[nodiscard]] std::vector<GeneratedEquipment> generatedEntityEquipment(const wire::nbt::NbtCompound& data);

/// The live synchronized fields this server mutates after an entity has spawned.
struct RuntimeEntityMetadata {
    std::int32_t airSupply = 0;
    std::optional<bool> baby;
    std::optional<std::int32_t> cubeSize;
    std::optional<std::int8_t> horseFlags;
    /// parity: `EndCrystal`'s beam and base, which change while a dragon is being called back.
    std::optional<std::optional<wire::protocol::play::BlockPos>> crystalBeam;
    std::optional<bool> crystalBottom;
    /// parity: `EnderDragon.DATA_PHASE`, which the client animates by.
    std::optional<std::int32_t> dragonPhase;
    /// parity: `ExperienceOrb.DATA_VALUE`, which picks the orb's size.
    std::optional<std::int32_t> orbValue;
    /// parity: `AreaEffectCloud`'s radius, waiting flag and particle.
    std::optional<float> cloudRadius;
    std::optional<bool> cloudWaiting;
    std::optional<wire::protocol::play::SetEntityDataPacket::ParticleValue> cloudParticle;
    /// parity: `FishingHook`'s hooked entity, as its id plus one, and whether a fish is biting.
    std::optional<std::int32_t> hookedEntity;
    std::optional<bool> hookBiting;
    /// parity: `LivingEntity.DATA_EFFECT_PARTICLES` and `DATA_EFFECT_AMBIENCE_ID`, the effect swirl.
    std::optional<wire::protocol::play::SetEntityDataPacket::ParticlesValue> effectParticles;
    std::optional<bool> effectsAmbient;
    /// parity: `Entity.DATA_POSE`, for the mobs whose pose this server changes, and the warden's
    /// `CLIENT_ANGER_LEVEL`.
    std::optional<wire::protocol::play::Pose> pose;
    std::optional<std::int32_t> wardenAnger;
    /// parity: `CopperGolem`'s weathering and the pose it works a chest in.
    std::optional<std::int32_t> copperWeather;
    std::optional<std::int32_t> copperPose;

    /// Initial non-default fields, or every field whose live value changed since `previous`.
    [[nodiscard]] std::vector<wire::protocol::play::SetEntityDataPacket::Entry>
    changesFrom(const RuntimeEntityMetadata* previous) const;

    friend bool operator==(const RuntimeEntityMetadata&, const RuntimeEntityMetadata&) = default;
};

[[nodiscard]] RuntimeEntityMetadata runtimeEntityMetadata(const ServerEntity& entity);

} // namespace basalt::server::runtime::entity
