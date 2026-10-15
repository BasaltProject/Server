#pragma once

#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/decoration/PlacementRegion.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration::sculk {

using PathSet = std::set<std::string, std::less<>>;

struct Direction {
    std::string_view property;
    std::int32_t x;
    std::int32_t y;
    std::int32_t z;
    std::int32_t axis;
    std::size_t oppositeIndex;

    friend bool operator==(const Direction& left, const Direction& right) {
        return left.oppositeIndex == right.oppositeIndex;
    }
};

inline constexpr std::array<Direction, 6> kDirections{
    Direction{"down", 0, -1, 0, 1, 1}, Direction{"up", 0, 1, 0, 1, 0},    Direction{"north", 0, 0, -1, 2, 3},
    Direction{"south", 0, 0, 1, 2, 2}, Direction{"west", -1, 0, 0, 0, 5}, Direction{"east", 1, 0, 0, 0, 4}};

[[nodiscard]] inline const Direction& opposite(const Direction& direction) {
    return kDirections[direction.oppositeIndex];
}

enum class SpreadType : std::uint8_t { SamePosition, SamePlane, WrapAround };

enum class SculkBehaviour : std::uint8_t { Default, Sculk, Vein };

struct SculkStates {
    std::int32_t sculk;
    std::int32_t vein;
    std::int32_t veinWaterlogged;
    std::int32_t catalyst;
    std::int32_t sensor;
    std::int32_t sensorWaterlogged;
    std::int32_t shrieker;
    std::int32_t shriekerWaterlogged;
};

[[nodiscard]] const SculkStates& states();

[[nodiscard]] const PathSet& veinSupports();

[[nodiscard]] const PathSet& spreadForbiddenSupports();

[[nodiscard]] const PathSet& nonSturdyBlocks();

[[nodiscard]] const PathSet& replaceable();

[[nodiscard]] const PathSet& replaceableWorldGen();

[[nodiscard]] std::string pathOf(std::int32_t stateId);

[[nodiscard]] bool isWaterBlock(std::int32_t stateId);

[[nodiscard]] bool hasSourceWater(std::int32_t stateId);

[[nodiscard]] DecorationPos offsetBy(const DecorationPos& position, const Direction& direction);

[[nodiscard]] std::int32_t manhattanDistance(const DecorationPos& a, const DecorationPos& b);

/// parity: `Util.shuffle`, the Fisher-Yates vanilla runs over its direction lists.
template <typename T>
void shuffle(std::vector<T>& values, random::RandomSource& random) {
    for (auto size = static_cast<std::int32_t>(values.size()); size >= 2; --size) {
        const std::int32_t swapTo = random.nextInt(size);
        std::swap(values[static_cast<std::size_t>(size - 1)], values[static_cast<std::size_t>(swapTo)]);
    }
}

class SculkRegion {
public:
    SculkRegion(Chunk& chunk, ExternalStateReader externalBlockState)
        : chunk_(&chunk), externalBlockState_(std::move(externalBlockState)) {}

    [[nodiscard]] Chunk& chunk() const noexcept { return *chunk_; }

    [[nodiscard]] std::optional<std::int32_t> get(const DecorationPos& position) const;

    void set(const DecorationPos& position, std::int32_t stateId);

    [[nodiscard]] std::string blockPath(const DecorationPos& position) const;

    [[nodiscard]] bool isAirOrWaterBlock(const DecorationPos& position) const;

    [[nodiscard]] static bool isAirOrWaterBlock(std::int32_t stateId);

    [[nodiscard]] static bool isAirOrWaterFluid(std::int32_t stateId);

    [[nodiscard]] static bool isVein(std::int32_t stateId);

    [[nodiscard]] static SculkBehaviour behaviour(std::int32_t stateId);

    [[nodiscard]] bool canSpreadFrom(const DecorationPos& position) const;

    [[nodiscard]] static bool isReplaceableForVein(std::int32_t stateId);

    [[nodiscard]] bool hasSturdyFace(const DecorationPos& position, const Direction& face) const;

    [[nodiscard]] bool hasFullCollision(const DecorationPos& position) const;

    [[nodiscard]] static bool hasFace(std::int32_t stateId, const Direction& direction);

    [[nodiscard]] std::optional<std::int32_t> withFace(std::int32_t oldStateId, const DecorationPos& position,
                                                       const Direction& direction) const;

private:
    Chunk* chunk_;
    ExternalStateReader externalBlockState_;
    std::vector<std::pair<DecorationPos, std::int32_t>> externalWrites_;
};

struct SpreadPosition {
    DecorationPos position;
    const Direction* face;
};

[[nodiscard]] SpreadPosition spreadPosition(const DecorationPos& source, const Direction& spreadDirection,
                                            const Direction& startingFace, SpreadType type);

[[nodiscard]] bool canSpreadInto(const SculkRegion& region, const DecorationPos& source, const SpreadPosition& spread);

std::int32_t spreadAll(SculkRegion& region, std::int32_t sourceStateId, const DecorationPos& source,
                       bool sameSpaceOnly);

void dischargeVein(SculkRegion& region, const DecorationPos& position, std::int32_t initialStateId);

bool placeMultifaceGrowth(SculkRegion& region, random::RandomSource& random, const DecorationPos& origin);

} // namespace basalt::server::world::terrain::decoration::sculk
