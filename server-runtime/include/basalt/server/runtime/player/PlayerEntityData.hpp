#pragma once

#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::player {

class PlayerSession;

/// Everything a player synchronizes to the clients that see them, and to their own.
///
/// parity: the `SynchedEntityData` of `Entity`, `LivingEntity`, `Avatar` and `Player`, in that
/// order. Diffed per viewer the way `ServerEntity.sendDirtyEntityData` sends the dirty ones, and
/// compared against the defined defaults for the pairing, as `getNonDefaultValues` does.
struct PlayerEntityData {
    static constexpr std::uint8_t kTicksFrozenIndex = 7;
    static constexpr std::uint8_t kHealthIndex = 9;
    static constexpr std::uint8_t kEffectParticlesIndex = 10;
    static constexpr std::uint8_t kEffectAmbienceIndex = 11;
    static constexpr std::uint8_t kArrowCountIndex = 12;
    static constexpr std::uint8_t kStingerCountIndex = 13;
    static constexpr std::uint8_t kMainHandIndex = 15;
    static constexpr std::uint8_t kScoreIndex = 18;
    static constexpr std::uint8_t kShoulderLeftIndex = 19;
    static constexpr std::uint8_t kShoulderRightIndex = 20;

    static constexpr std::uint8_t kLivingFlagUsingItem = 0x01;
    static constexpr std::uint8_t kLivingFlagOffhand = 0x02;
    static constexpr std::uint8_t kLivingFlagSpinAttack = 0x04;

    std::uint8_t sharedFlags = 0;
    std::int32_t airSupply = 300;
    wire::protocol::play::Pose pose = wire::protocol::play::Pose::Standing;
    std::int32_t ticksFrozen = 0;
    std::uint8_t livingFlags = 0;
    float health = 1.0F;
    wire::protocol::play::SetEntityDataPacket::ParticlesValue effectParticles;
    bool effectsAmbient = false;
    std::int32_t arrowCount = 0;
    std::int32_t stingerCount = 0;
    std::optional<wire::protocol::play::BlockPos> sleepingPos;
    std::int32_t mainArm = 1;
    std::int8_t skinParts = 0;
    float absorption = 0.0F;
    std::int32_t score = 0;
    std::optional<std::int32_t> shoulderLeft;
    std::optional<std::int32_t> shoulderRight;

    /// What the player's own values are now.
    [[nodiscard]] static PlayerEntityData of(const PlayerSession& session);

    /// The entries that differ from `previous`, or from the defined defaults when there is none.
    [[nodiscard]] std::vector<wire::protocol::play::SetEntityDataPacket::Entry>
    changesFrom(const PlayerEntityData* previous) const;

    friend bool operator==(const PlayerEntityData&, const PlayerEntityData&) = default;
};

/// parity: `Player.extractParrotVariant`, the variant of a parrot's saved tag, or nothing.
[[nodiscard]] std::optional<std::int32_t> parrotVariantOf(const std::optional<wire::nbt::NbtCompound>& tag);

/// A tick of the player's body: shedding arrows and stingers, `updateSwimming` and
/// `updatePlayerPose`, with `blockAt` answering what is where.
void tickBody(PlayerSession& session, const entity::entityCollision::BlockAt& blockAt);

} // namespace basalt::server::runtime::player
