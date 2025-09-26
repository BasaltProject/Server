#pragma once

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace basalt::server::world::terrain::decoration::tree {

/// parity: `FoliagePlacer.FoliageAttachment`. Coordinates are world block coordinates.
struct FoliageAttachment {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t radiusOffset = 0;
    bool doubleTrunk = false;

    friend bool operator==(const FoliageAttachment&, const FoliageAttachment&) = default;
};

/// parity: `TwoLayersFeatureSize` and `ThreeLayersFeatureSize`.
///
/// A value type with a kind tag rather than a class hierarchy: this build has RTTI off, and the two
/// shapes together are five ints.
class FeatureSize {
public:
    [[nodiscard]] static constexpr FeatureSize twoLayers(std::int32_t limit, std::int32_t lowerSize,
                                                         std::int32_t upperSize,
                                                         std::optional<std::int32_t> minClippedHeight = {}) {
        FeatureSize size;
        size.kind_ = Kind::TwoLayers;
        size.limit_ = limit;
        size.lowerSize_ = lowerSize;
        size.upperSize_ = upperSize;
        size.minClippedHeight_ = minClippedHeight;
        return size;
    }

    [[nodiscard]] static constexpr FeatureSize threeLayers(std::int32_t limit, std::int32_t upperLimit,
                                                           std::int32_t lowerSize, std::int32_t middleSize,
                                                           std::int32_t upperSize) {
        FeatureSize size;
        size.kind_ = Kind::ThreeLayers;
        size.limit_ = limit;
        size.upperLimit_ = upperLimit;
        size.lowerSize_ = lowerSize;
        size.middleSize_ = middleSize;
        size.upperSize_ = upperSize;
        return size;
    }

    [[nodiscard]] constexpr std::optional<std::int32_t> minClippedHeight() const { return minClippedHeight_; }

    [[nodiscard]] constexpr std::int32_t sizeAtHeight(std::int32_t treeHeight, std::int32_t yo) const {
        if (kind_ == Kind::TwoLayers) {
            return yo < limit_ ? lowerSize_ : upperSize_;
        }
        if (yo < limit_) {
            return lowerSize_;
        }
        // parity: the upper band is measured from the top down, so it depends on the tree's own
        // height rather than on a fixed y.
        if (yo >= treeHeight - upperLimit_) {
            return upperSize_;
        }
        return middleSize_;
    }

private:
    enum class Kind : std::uint8_t { TwoLayers, ThreeLayers };

    Kind kind_ = Kind::TwoLayers;
    std::int32_t limit_ = 0;
    std::int32_t upperLimit_ = 0;
    std::int32_t lowerSize_ = 0;
    std::int32_t middleSize_ = 0;
    std::int32_t upperSize_ = 0;
    std::optional<std::int32_t> minClippedHeight_;
};

/// The tree decorators a configuration may carry, as a closed set.
namespace decorator {

struct AlterGroundToPodzol {
    friend bool operator==(const AlterGroundToPodzol&, const AlterGroundToPodzol&) = default;
};

struct TrunkVines {
    friend bool operator==(const TrunkVines&, const TrunkVines&) = default;
};

struct LeafVines {
    float probability = 0.0F;
    friend bool operator==(const LeafVines&, const LeafVines&) = default;
};

struct Cocoa {
    float probability = 0.0F;
    friend bool operator==(const Cocoa&, const Cocoa&) = default;
};

struct PaleMoss {
    float leavesProbability = 0.0F;
    float trunkProbability = 0.0F;
    float groundProbability = 0.0F;
    friend bool operator==(const PaleMoss&, const PaleMoss&) = default;
};

struct CreakingHeart {
    float probability = 0.0F;
    friend bool operator==(const CreakingHeart&, const CreakingHeart&) = default;
};

struct MangrovePropagules {
    float probability = 0.0F;
    std::int32_t exclusionRadiusXZ = 0;
    std::int32_t exclusionRadiusY = 0;
    std::int32_t requiredEmptyBlocks = 0;
    friend bool operator==(const MangrovePropagules&, const MangrovePropagules&) = default;
};

struct GroundLeafLitter {
    std::int32_t tries = 0;
    std::int32_t radius = 0;
    std::int32_t height = 0;
    std::int32_t maxSegmentAmount = 0;
    friend bool operator==(const GroundLeafLitter&, const GroundLeafLitter&) = default;
};

struct Beehive {
    float probability = 0.0F;
    friend bool operator==(const Beehive&, const Beehive&) = default;
};

} // namespace decorator

using TreeDecoratorConfiguration =
    std::variant<decorator::AlterGroundToPodzol, decorator::TrunkVines, decorator::LeafVines, decorator::Cocoa,
                 decorator::PaleMoss, decorator::CreakingHeart, decorator::MangrovePropagules,
                 decorator::GroundLeafLitter, decorator::Beehive>;

} // namespace basalt::server::world::terrain::decoration::tree
