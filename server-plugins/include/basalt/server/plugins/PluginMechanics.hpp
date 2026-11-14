#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins::mechanics {

using Point = std::array<double, 3>;

/// Colliders are 7 doubles each (box min then max, shape), a box is 6, a body is 6 (position, width, height, yaw).
[[nodiscard]] Point collide(std::span<const double> colliders, std::span<const double> box, const Point& movement);
[[nodiscard]] std::optional<Point> stepUp(std::span<const double> colliders, std::span<const double> grounded,
                                          double groundedOffsetY, const Point& movement, float maxUpStep,
                                          double reachSquared);
[[nodiscard]] double offspringAttribute(double parentA, double parentB, double min, double max, double first,
                                        double second, double third);
[[nodiscard]] float levelValue(std::span<const double> program, int level);

[[nodiscard]] float afterArmour(float damage, float armour, float toughness, float fractionAdd);
[[nodiscard]] float afterProtection(float damage, float protection);
[[nodiscard]] float afterResistance(float damage, std::int32_t amplifier);
[[nodiscard]] bool scalesWithDifficulty(std::string_view scaling, bool causedByLivingNonPlayer);
/// Health then absorption left after absorbing `damage`.
[[nodiscard]] std::array<float, 2> absorb(float damage, float absorption);

/// xRotO, yRotO, xRot, yRot, or nothing when the projectile does not turn.
[[nodiscard]] std::optional<std::array<float, 4>> rotateTowardsMovement(double motionX, double motionY, double motionZ,
                                                                        float xRotO, float yRotO, float xRot,
                                                                        float yRot, float speed);
/// Candidates are 9 doubles each (box, pick radius, pickable from inside, root vehicle); a hit is index then point.
[[nodiscard]] std::optional<std::array<double, 4>> closestHit(std::span<const double> candidates, const Point& from,
                                                              const Point& to, float margin);
[[nodiscard]] std::optional<std::array<double, 4>> pickedHit(std::span<const double> candidates,
                                                             std::int32_t exceptRootVehicle, const Point& from,
                                                             const Point& to, double maxValue);
[[nodiscard]] float projectileMargin(std::int32_t tickCount);
[[nodiscard]] std::array<double, 6> shulkerBox(float size, std::int32_t direction, float progressFrom, float progressTo,
                                               const Point& position);

[[nodiscard]] float angularFriction(bool onGround, float blockFriction, bool inLiquid);
/// Torque then the push, or nothing when the leash does not pull.
[[nodiscard]] std::optional<std::array<double, 4>> leashPull(std::span<const double> self,
                                                             std::span<const double> holder, bool quad, double slack,
                                                             const Point& selfMotion, const Point& holderMotion,
                                                             const Point& knownMotion);

[[nodiscard]] std::vector<std::string> creativeTabItems(std::string_view tab);

[[nodiscard]] std::optional<std::vector<std::uint8_t>> regionRead(const std::string& folder, std::int32_t chunkX,
                                                                  std::int32_t chunkZ);
void regionWrite(const std::string& folder, std::int32_t chunkX, std::int32_t chunkZ, std::vector<std::uint8_t> data,
                 std::int32_t timestamp);
[[nodiscard]] std::int32_t regionTimestamp(const std::string& folder, std::int32_t chunkX, std::int32_t chunkZ);
/// Flushes the folder's regions, and also closes them when `release` is set.
bool regionFlush(const std::string& folder, bool release);

struct BiomePoints {
    std::vector<std::int64_t> parameters;
    std::vector<std::string> names;
};

/// Thirteen values per biome point: six min/max pairs then the offset.
[[nodiscard]] BiomePoints overworldBiomes();

} // namespace basalt::server::plugins::mechanics
