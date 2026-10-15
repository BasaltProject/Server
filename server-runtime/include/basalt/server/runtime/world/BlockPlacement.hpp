#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace basalt::server::runtime::world {

namespace blockPlacement {

using Box = std::array<double, 6>;

inline constexpr double kPlayerWidth = 0.6;
inline constexpr double kPlayerHeight = 1.8;
inline constexpr double kSneakingHeight = 1.5;

struct Placement {
    wire::protocol::play::Direction face = wire::protocol::play::Direction::Up;
    float clickY = 0.5F;
    float clickX = 0.5F;
    float clickZ = 0.5F;
    float yaw = 0.0F;
    float pitch = 0.0F;
    std::int32_t replacing = 0;
    const WorldState* world = nullptr;
    /// Needed only to ask the world for power, which a handful of blocks do as they land.
    std::string_view worldName;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

[[nodiscard]] std::array<wire::protocol::play::Direction, 6>
nearestLookingDirections(float yaw, float pitch, wire::protocol::play::Direction clickedFace);

[[nodiscard]] std::int32_t stateFor(const wire::common::Identifier& block, std::int32_t defaultStateId,
                                    const Placement& placement, random::RandomSource& random);

/// parity: `BlockItem.canPlace` over `Level.isUnobstructed`.
[[nodiscard]] bool isObstructed(std::int32_t stateId, std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ,
                                std::span<const Box> boxes);

[[nodiscard]] Box playerBox(double x, double y, double z, bool sneaking);

[[nodiscard]] wire::protocol::play::Direction playerDirection(float yaw);

[[nodiscard]] wire::protocol::play::Direction nearestLooking(float yaw, float pitch);

} // namespace blockPlacement

namespace wallVariants {

[[nodiscard]] std::optional<wire::common::Identifier> of(const wire::common::Identifier& block);

} // namespace wallVariants

/// parity: `ScaffoldingBlockItem.updatePlacementContext`.
namespace scaffoldingPlacement {

inline constexpr std::int32_t kReach = 7;

[[nodiscard]] std::optional<std::array<std::int32_t, 3>>
target(const WorldState& world, std::int32_t minY, std::int32_t maxY, std::int32_t clickedX, std::int32_t clickedY,
       std::int32_t clickedZ, wire::protocol::play::Direction face, float yaw, bool sneaking, bool inside);

} // namespace scaffoldingPlacement

namespace mossCarpetTopper {

[[nodiscard]] std::optional<std::int32_t> above(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                const std::function<bool()>& coin);

} // namespace mossCarpetTopper

struct SecondHalf {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t stateId = 0;

    friend bool operator==(const SecondHalf&, const SecondHalf&) = default;
};

namespace doubleBlocks {

[[nodiscard]] std::optional<SecondHalf> secondHalf(std::int32_t stateId, std::int32_t x, std::int32_t y,
                                                   std::int32_t z);

[[nodiscard]] std::int32_t firstHalf(std::int32_t stateId);

} // namespace doubleBlocks

} // namespace basalt::server::runtime::world
