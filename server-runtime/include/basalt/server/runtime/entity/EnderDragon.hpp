#pragma once

#include <basalt/server/runtime/entity/DragonState.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// The dragon of the End.
///
/// parity: `EnderDragon`, whose eight `EnderDragonPart`s are entities of their own. A client numbers
/// them itself, as the eight ids after the dragon's (`recreateFromPacket`), and hits them by those
/// ids, so the server hands the dragon a block of nine.
namespace enderDragon {

inline constexpr std::int32_t kPartCount = 8;

/// parity: `EnderDragon.createAttributes`.
inline constexpr double kMaxHealth = 200.0;

/// parity: `EnderDragon.DATA_PHASE`, right after the mob's own flags.
inline constexpr std::uint8_t kPhaseDataIndex = 16;

/// parity: the keys `EnderDragon.addAdditionalSaveData` writes.
inline constexpr std::string_view kPhaseKey = "DragonPhase";
inline constexpr std::string_view kDeathTimeKey = "DragonDeathTime";
inline constexpr std::string_view kSittingDamageKey = "sitting_damage_received";

[[nodiscard]] bool isDragon(const wire::common::Identifier& type);

/// How many consecutive entity ids an entity of this type takes.
[[nodiscard]] std::int32_t idSpan(const wire::common::Identifier& type);

/// The part a client means by that entity id, or nothing when the id is not one of this dragon's.
[[nodiscard]] std::optional<Part> partByEntityId(std::int32_t dragonId, std::int32_t entityId);

/// A player the dragon can pick, as it sees one.
struct Target {
    std::int32_t entityId = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double height = 0.0;
};

/// parity: the `TargetingConditions` the phases use. A zero range reaches everywhere.
struct TargetQuery {
    double range = 0.0;
    bool needsSight = true;
    /// parity: `DragonSittingScanningPhase`'s own selector, a player at most this far above or below.
    std::optional<double> maxHeightGap;
};

/// What a dragon asks of the world it flies in.
struct DragonWorld {
    random::RandomSource* random = nullptr;
    /// parity: `getHeightmapPos(MOTION_BLOCKING_NO_LEAVES)` and `MOTION_BLOCKING`, as a height.
    std::function<std::int32_t(std::int32_t x, std::int32_t z)> heightNoLeaves;
    std::function<std::int32_t(std::int32_t x, std::int32_t z)> heightMotionBlocking;
    /// parity: `Level.getNearestPlayer(conditions, dragon, x, y, z)`.
    std::function<std::optional<Target>(double x, double y, double z, const TargetQuery& query)> nearestPlayer;
    /// The player with that id, if still there.
    std::function<std::optional<Target>(std::int32_t entityId)> playerById;
    /// parity: `LivingEntity.hasLineOfSight`.
    std::function<bool(const Target& target)> canSee;
    /// parity: `EnderDragonFight.aliveCrystals`, or nothing when the dragon has no fight.
    std::optional<std::int32_t> aliveCrystals;
    /// parity: `Level.isEmptyBlock`.
    std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z)> isEmptyBlock;
    /// A dragon fireball leaving `from` towards `direction`, already normalised.
    std::function<void(const Vec3& from, const Vec3& direction)> shootFireball;
    /// The breath cloud the sitting dragon lays down; answers its id.
    std::function<std::optional<std::int32_t>(const Vec3& at)> breathe;
    std::function<void(std::int32_t cloudId)> stopBreathing;
};

/// A dragon's state, with the parts laid out where a fresh one has them.
[[nodiscard]] DragonState freshState(const ServerEntity& dragon);

/// parity: `EnderDragonPhaseManager.setPhase`: nothing when it is already there, otherwise the old
/// phase ends and the new one begins.
void setPhase(DragonState& state, Phase phase, DragonWorld& world);

/// The same, from a hit, where the breath cloud a phase leaves is removed on the dragon's next turn.
void setPhase(DragonState& state, Phase phase);

/// One server turn of flight. parity: `EnderDragon.aiStep` up to where the parts are placed; what
/// the parts then do to the world is the caller's, between `placeBodyAndWings` and `placeRest`.
void steer(ServerEntity& dragon, DragonState& state, DragonWorld& world);

void placeBodyAndWings(const ServerEntity& dragon, DragonState& state);

void placeRest(const ServerEntity& dragon, DragonState& state);

/// A part's box, as `{minX, minY, minZ, maxX, maxY, maxZ}`.
[[nodiscard]] std::array<double, 6> partBox(const DragonState& state, Part part);

/// What is left of a hit on a part once the phase and the part have had their say, or nothing when
/// it does not land. parity: `EnderDragon.hurt(level, part, source, damage)` before `reallyHurt`.
///
/// `projectile` is an arrow or a wind charge, which a sitting dragon shrugs off.
[[nodiscard]] std::optional<float> damageFor(const DragonState& state, Part part, float damage, bool projectile);

/// What landing that hit does to the phase. parity: the sitting dragon takes off after a quarter.
void afterHurt(DragonState& state, float healthBefore, float healthAfter);

/// parity: `EnderDragon.handleKillingBlow`: a flying dragon does not die, it goes to die.
[[nodiscard]] bool refusesDeath(DragonState& state);

/// The phase's own server tick. parity: `DragonPhaseInstance.doServerTick`.
void tickPhase(ServerEntity& dragon, DragonState& state, DragonWorld& world);

/// parity: `DragonHoldingPatternPhase.onCrystalDestroyed`: a circling dragon goes after whoever broke
/// a crystal.
void strafeAfterCrystal(ServerEntity& dragon, DragonState& state, DragonWorld& world, const Target& player);

/// parity: `DragonPhaseInstance.getFlyTargetLocation`, `getFlySpeed` and `getTurnSpeed`.
[[nodiscard]] std::optional<Vec3> flyTarget(const DragonState& state);

[[nodiscard]] float flySpeed(const DragonState& state);

[[nodiscard]] float turnSpeed(const ServerEntity& dragon, const DragonState& state);

/// parity: `Entity.calculateViewVector` for this pitch and the dragon's own heading.
[[nodiscard]] Vec3 viewVector(float pitch, float yaw);

/// parity: `EnderDragon.getHeadLookVector`.
[[nodiscard]] Vec3 headLookVector(const ServerEntity& dragon, const DragonState& state, const DragonWorld& world);

} // namespace enderDragon

} // namespace basalt::server::runtime::entity
