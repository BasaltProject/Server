#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::player {

namespace spear {

struct Condition {
    std::int32_t maxDurationTicks = 0;
    float minSpeed = 0.0F;
    float minRelativeSpeed = 0.0F;

    [[nodiscard]] bool test(std::int32_t ticksUsed, double attackerSpeed, double relativeSpeed,
                            double entityFactor) const;
};

struct Spear {
    std::string_view item;
    std::int32_t contactCooldownTicks = 10;
    std::int32_t delayTicks = 0;
    Condition dismount;
    Condition knockback;
    Condition damage;
    float damageMultiplier = 1.0F;
    std::string_view useSound;
    std::string_view hitSound;
    std::string_view attackSound;
};

struct Range {
    float minReach = 2.0F;
    float maxReach = 4.5F;
    float minCreativeReach = 2.0F;
    float maxCreativeReach = 6.5F;
    float hitboxMargin = 0.125F;
};

inline constexpr Range kRange{};

inline constexpr float kMinimumAttackCharge = 1.0F;
inline constexpr std::int32_t kJabTolerance = 5;

inline constexpr std::int32_t kUseDuration = 72000;

inline constexpr float kPlayerFactor = 1.0F;

[[nodiscard]] const Spear* of(std::string_view itemId);

/// parity: `Player.cannotAttackWithItem`.
[[nodiscard]] bool tooSoon(std::int32_t ticksSinceSwing, double attackSpeed, std::int32_t tolerance);

[[nodiscard]] std::array<double, 3> lookOf(float yaw, float pitch);

struct Box {
    double minX = 0.0;
    double minY = 0.0;
    double minZ = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    double maxZ = 0.0;
};

struct Candidate {
    std::int32_t id = 0;
    Box box;
};

[[nodiscard]] std::vector<std::int32_t> hitsAlong(world::WorldState& world, const std::array<double, 3>& eye,
                                                  const std::array<double, 3>& look, double movementAlongLook,
                                                  bool creative, std::span<const Candidate> candidates);

} // namespace spear

} // namespace basalt::server::runtime::player
