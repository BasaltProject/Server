#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace basalt::server::world::climate {

/// The seven quantised climate axes: temperature, humidity, continentalness, erosion, depth,
/// weirdness, and the offset every target implicitly asks for at zero.
inline constexpr std::size_t kDimensions = 7;

/// parity: `Climate.Parameter`, a quantised span along one axis.
///
/// Values are pre-multiplied by the quantisation factor and truncated to an integer, matching
/// vanilla exactly, its rounding included: truncation toward zero rather than to nearest.
struct Parameter {
    std::int64_t min = 0;
    std::int64_t max = 0;

    [[nodiscard]] constexpr std::int64_t distance(std::int64_t target) const noexcept {
        const std::int64_t above = target - max;
        const std::int64_t below = min - target;
        return above > 0 ? above : (below > 0 ? below : 0);
    }

    [[nodiscard]] constexpr std::int64_t distance(const Parameter& target) const noexcept {
        const std::int64_t above = target.min - max;
        const std::int64_t below = min - target.max;
        return above > 0 ? above : (below > 0 ? below : 0);
    }

    [[nodiscard]] constexpr Parameter span(const Parameter& other) const noexcept {
        return {min < other.min ? min : other.min, max > other.max ? max : other.max};
    }

    friend bool operator==(const Parameter&, const Parameter&) = default;
};

inline constexpr float kQuantizationFactor = 10000.0F;

[[nodiscard]] constexpr std::int64_t quantizeCoord(float coord) noexcept {
    return static_cast<std::int64_t>(coord * kQuantizationFactor);
}

[[nodiscard]] constexpr float unquantizeCoord(std::int64_t coord) noexcept {
    return static_cast<float>(coord) / kQuantizationFactor;
}

[[nodiscard]] Parameter parameterSpan(float min, float max);

[[nodiscard]] inline Parameter parameterPointAt(float value) { return parameterSpan(value, value); }

/// parity: `Climate.TargetPoint`, a single sampled climate point, quantised.
struct TargetPoint {
    std::int64_t temperature = 0;
    std::int64_t humidity = 0;
    std::int64_t continentalness = 0;
    std::int64_t erosion = 0;
    std::int64_t depth = 0;
    std::int64_t weirdness = 0;

    /// The offset axis has no target value: it is always compared against zero, which is vanilla's
    /// own seventh entry.
    [[nodiscard]] std::array<std::int64_t, kDimensions> toArray() const noexcept {
        return {temperature, humidity, continentalness, erosion, depth, weirdness, 0};
    }

    friend bool operator==(const TargetPoint&, const TargetPoint&) = default;
};

[[nodiscard]] TargetPoint target(float temperature, float humidity, float continentalness, float erosion, float depth,
                                 float weirdness);

/// parity: `Climate.ParameterPoint`, one biome's climate niche.
struct ParameterPoint {
    Parameter temperature;
    Parameter humidity;
    Parameter continentalness;
    Parameter erosion;
    Parameter depth;
    Parameter weirdness;
    std::int64_t offset = 0;

    /// Squared distance to a target: lower is a better match.
    [[nodiscard]] std::int64_t fitness(const TargetPoint& point) const;

    [[nodiscard]] std::array<Parameter, kDimensions> parameterSpace() const;

    friend bool operator==(const ParameterPoint&, const ParameterPoint&) = default;
};

[[nodiscard]] ParameterPoint parameters(float temperature, float humidity, float continentalness, float erosion,
                                        float depth, float weirdness, float offset);

/// parity: `Climate.RTree`, a bounding-volume tree over the seven axes so the closest parameter
/// point is a descent rather than a full scan.
///
/// The tree is built once and read-only after, so it needs no lock. Each search is seeded with the
/// last leaf *this thread* found, as vanilla's own thread local does. That is not only faster: a
/// search seeded differently resolves *tied* targets differently, and a tie between two equally
/// distant regions is decided by whichever the descent reaches first.
template <typename T>
class RTree {
public:
    /// Refuses an empty list or a point of the wrong arity: both are table bugs, and a tree with no
    /// leaves has no biome to answer with.
    [[nodiscard]] static std::unique_ptr<RTree> create(std::vector<std::pair<ParameterPoint, T>> values);

    [[nodiscard]] const T& search(const std::array<std::int64_t, kDimensions>& target) const;

private:
    struct Leaf;

    struct Node {
        virtual ~Node() = default;
        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;
        Node(Node&&) = delete;
        Node& operator=(Node&&) = delete;

        explicit Node(std::array<Parameter, kDimensions> space) : parameterSpace(space) {}

        [[nodiscard]] virtual const Leaf* search(const std::array<std::int64_t, kDimensions>& target,
                                                 const Leaf* candidate) const = 0;

        [[nodiscard]] std::int64_t distance(const std::array<std::int64_t, kDimensions>& target) const;

        std::array<Parameter, kDimensions> parameterSpace;
    };

    struct Leaf final : Node {
        Leaf(const ParameterPoint& point, T value) : Node(point.parameterSpace()), value(std::move(value)) {}

        [[nodiscard]] const Leaf* search(const std::array<std::int64_t, kDimensions>&, const Leaf*) const override {
            return this;
        }

        T value;
    };

    struct SubTree final : Node {
        explicit SubTree(std::vector<std::unique_ptr<Node>> children);

        [[nodiscard]] const Leaf* search(const std::array<std::int64_t, kDimensions>& target,
                                         const Leaf* candidate) const override;

        std::vector<std::unique_ptr<Node>> children;
    };

    explicit RTree(std::unique_ptr<Node> root) : root_(std::move(root)) {}

    std::unique_ptr<Node> root_;
    /// The previous leaf this thread found in this tree, which is what seeds the next search.
    mutable std::size_t slot_;
};

} // namespace basalt::server::world::climate

#include <basalt/server/world/climate/RTree.inl>
