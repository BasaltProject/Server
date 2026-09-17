#include <basalt/server/runtime/entity/DragonNavigation.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <bit>
#include <cmath>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

namespace basalt::server::runtime::entity::enderDragon {
namespace {

constexpr std::int32_t kOuterRing = 12;
constexpr std::int32_t kMiddleRing = 20;

/// parity: `Node.createHash`.
[[nodiscard]] std::int32_t nodeHash(std::int32_t x, std::int32_t y, std::int32_t z) noexcept {
    return static_cast<std::int32_t>(
        (static_cast<std::uint32_t>(y) & 0xFFU) | ((static_cast<std::uint32_t>(x) & 32767U) << 8U) |
        ((static_cast<std::uint32_t>(z) & 32767U) << 24U) | (x < 0 ? 0x80000000U : 0U) | (z < 0 ? 32768U : 0U));
}

[[nodiscard]] Node makeNode(std::int32_t x, std::int32_t y, std::int32_t z) noexcept {
    return Node{.x = x, .y = y, .z = z, .hash = nodeHash(x, y, z)};
}

/// parity: `Node.distanceTo`, in floats.
[[nodiscard]] float distance(const Node& from, const Node& to) noexcept {
    const auto xd = static_cast<float>(to.x - from.x);
    const auto yd = static_cast<float>(to.y - from.y);
    const auto zd = static_cast<float>(to.z - from.z);
    return static_cast<float>(std::sqrt(static_cast<double>((xd * xd) + (yd * yd) + (zd * zd))));
}

[[nodiscard]] float distanceSqr(const Node& from, const Node& to) noexcept {
    const auto xd = static_cast<float>(to.x - from.x);
    const auto yd = static_cast<float>(to.y - from.y);
    const auto zd = static_cast<float>(to.z - from.z);
    return (xd * xd) + (yd * yd) + (zd * zd);
}

[[nodiscard]] bool sameNode(const Node& a, const Node& b) noexcept {
    return a.hash == b.hash && a.x == b.x && a.y == b.y && a.z == b.z;
}

/// parity: `BinaryHeap`, whose tie-breaking decides which of two equal routes the dragon takes.
class NodeHeap {
public:
    explicit NodeHeap(std::vector<Node>& pool) : pool_(&pool) {}

    [[nodiscard]] bool isEmpty() const noexcept { return heap_.empty(); }

    void insert(std::int32_t index) {
        heap_.push_back(index);
        node(index).heapIdx = static_cast<std::int32_t>(heap_.size() - 1);
        upHeap(static_cast<std::int32_t>(heap_.size() - 1));
    }

    [[nodiscard]] std::int32_t pop() {
        const std::int32_t popped = heap_.front();
        heap_.front() = heap_.back();
        heap_.pop_back();
        if (!heap_.empty()) {
            downHeap(0);
        }
        node(popped).heapIdx = -1;
        return popped;
    }

    void changeCost(std::int32_t index, float cost) {
        const float old = node(index).f;
        node(index).f = cost;
        if (cost < old) {
            upHeap(node(index).heapIdx);
        } else {
            downHeap(node(index).heapIdx);
        }
    }

private:
    [[nodiscard]] Node& node(std::int32_t index) { return (*pool_)[static_cast<std::size_t>(index)]; }

    void place(std::int32_t slot, std::int32_t index) {
        heap_[static_cast<std::size_t>(slot)] = index;
        node(index).heapIdx = slot;
    }

    void upHeap(std::int32_t slot) {
        const std::int32_t moving = heap_[static_cast<std::size_t>(slot)];
        const float cost = node(moving).f;
        while (slot > 0) {
            const std::int32_t parentSlot = (slot - 1) >> 1;
            const std::int32_t parent = heap_[static_cast<std::size_t>(parentSlot)];
            if (!(cost < node(parent).f)) {
                break;
            }
            place(slot, parent);
            slot = parentSlot;
        }
        place(slot, moving);
    }

    void downHeap(std::int32_t slot) {
        const std::int32_t moving = heap_[static_cast<std::size_t>(slot)];
        const float cost = node(moving).f;
        const auto size = static_cast<std::int32_t>(heap_.size());
        while (true) {
            const std::int32_t leftSlot = 1 + (slot << 1);
            const std::int32_t rightSlot = leftSlot + 1;
            if (leftSlot >= size) {
                break;
            }
            const std::int32_t left = heap_[static_cast<std::size_t>(leftSlot)];
            const float leftCost = node(left).f;
            const std::int32_t right = rightSlot >= size ? -1 : heap_[static_cast<std::size_t>(rightSlot)];
            const float rightCost = right < 0 ? std::numeric_limits<float>::infinity() : node(right).f;
            if (leftCost < rightCost) {
                if (!(leftCost < cost)) {
                    break;
                }
                place(slot, left);
                slot = leftSlot;
            } else {
                if (!(rightCost < cost)) {
                    break;
                }
                place(slot, right);
                slot = rightSlot;
            }
        }
        place(slot, moving);
    }

    std::vector<Node>* pool_;
    std::vector<std::int32_t> heap_;
};

/// parity: `Mth.ASIN_TAB` and `Mth.COS_TAB`.
struct AtanTables {
    std::array<double, 257> asin{};
    std::array<double, 257> cos{};

    AtanTables() {
        for (std::size_t index = 0; index < 257; ++index) {
            const double value = static_cast<double>(index) / 256.0;
            asin[index] = std::asin(value);
            cos[index] = std::cos(asin[index]);
        }
    }
};

} // namespace

Phase phaseById(std::int32_t id) noexcept {
    return id >= 0 && id <= static_cast<std::int32_t>(Phase::Hover) ? static_cast<Phase>(id) : Phase::HoldingPattern;
}

bool isSitting(Phase phase) noexcept {
    return phase == Phase::SittingFlaming || phase == Phase::SittingScanning || phase == Phase::SittingAttacking ||
           phase == Phase::Hover;
}

void FlightHistory::record(double y, float yRot) {
    const Sample sample{.y = y, .yRot = yRot};
    if (head_ < 0) {
        samples_.fill(sample);
    }
    if (++head_ == kLength) {
        head_ = 0;
    }
    samples_[static_cast<std::size_t>(head_)] = sample;
}

const FlightHistory::Sample& FlightHistory::get(std::int32_t delay) const {
    return samples_[static_cast<std::size_t>((head_ - delay) & (kLength - 1))];
}

float wrapDegrees(float degrees) noexcept {
    float wrapped = std::fmod(degrees, 360.0F);
    if (wrapped >= 180.0F) {
        wrapped -= 360.0F;
    }
    if (wrapped < -180.0F) {
        wrapped += 360.0F;
    }
    return wrapped;
}

double wrapDegrees(double degrees) noexcept {
    double wrapped = std::fmod(degrees, 360.0);
    if (wrapped >= 180.0) {
        wrapped -= 360.0;
    }
    if (wrapped < -180.0) {
        wrapped += 360.0;
    }
    return wrapped;
}

double vanillaAtan2(double y, double x) {
    // parity: `Mth.atan2`, a table lookup and a cubic correction rather than the libm one.
    static const AtanTables kTables;
    const double squared = (x * x) + (y * y);
    if (std::isnan(squared)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const bool negY = y < 0.0;
    if (negY) {
        y = -y;
    }
    const bool negX = x < 0.0;
    if (negX) {
        x = -x;
    }
    const bool steep = y > x;
    if (steep) {
        std::swap(x, y);
    }
    const double inverse = wire::common::mth::fastInvSqrt(squared);
    x *= inverse;
    y *= inverse;
    // 2^44 has a unit in the last place of 1/256, so adding it rounds y to the table's step, and the
    // low bits of the sum are the index.
    constexpr double kFracBias = 17592186044416.0;
    const double biased = kFracBias + y;
    const auto index = static_cast<std::size_t>(static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(biased)));
    const double phi = kTables.asin[index];
    const double cosPhi = kTables.cos[index];
    const double sinPhi = biased - kFracBias;
    const double sd = (y * cosPhi) - (x * sinPhi);
    const double correction = (6.0 + (sd * sd)) * sd * 0.16666666666666666;
    double theta = phi + correction;
    if (steep) {
        theta = (std::numbers::pi / 2) - theta;
    }
    if (negX) {
        theta = std::numbers::pi - theta;
    }
    if (negY) {
        theta = -theta;
    }
    return theta;
}

void buildNodes(DragonState& state, const std::function<std::int32_t(std::int32_t x, std::int32_t z)>& heightNoLeaves) {
    if (state.nodesReady) {
        return;
    }
    // parity: `EnderDragon.findClosestNode`, three rings of 12, 8 and 4 around the fight origin's
    // column, each lifted over whatever stands under it.
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(kNodeCount); ++i) {
        std::int32_t yAdjustment = 5;
        std::int32_t nodeX = 0;
        std::int32_t nodeZ = 0;
        const auto pi = static_cast<float>(std::numbers::pi);
        if (i < kOuterRing) {
            const float angle = 2.0F * (-pi + (pi / 12.0F) * static_cast<float>(i));
            nodeX = wire::common::mth::floor(static_cast<double>(60.0F * wire::common::mth::cos(angle)));
            nodeZ = wire::common::mth::floor(static_cast<double>(60.0F * wire::common::mth::sin(angle)));
        } else if (i < kMiddleRing) {
            const float angle = 2.0F * (-pi + (pi / 8.0F) * static_cast<float>(i - kOuterRing));
            nodeX = wire::common::mth::floor(static_cast<double>(40.0F * wire::common::mth::cos(angle)));
            nodeZ = wire::common::mth::floor(static_cast<double>(40.0F * wire::common::mth::sin(angle)));
            yAdjustment += 10;
        } else {
            const float angle = 2.0F * (-pi + (pi / 4.0F) * static_cast<float>(i - kMiddleRing));
            nodeX = wire::common::mth::floor(static_cast<double>(20.0F * wire::common::mth::cos(angle)));
            nodeZ = wire::common::mth::floor(static_cast<double>(20.0F * wire::common::mth::sin(angle)));
        }
        const std::int32_t nodeY = std::max(73, heightNoLeaves(nodeX, nodeZ) + yAdjustment);
        state.nodes[static_cast<std::size_t>(i)] = makeNode(nodeX, nodeY, nodeZ);
    }
    state.adjacency = {6146,    8197,     8202,    16404,   32808,    32848,    65696,    131392,
                       131712,  263424,   526848,  525313,  1581057,  3166214,  2138120,  6373424,
                       4358208, 12910976, 9044480, 9706496, 15216640, 13688832, 11763712, 8257536};
    state.nodesReady = true;
}

std::int32_t closestNode(const DragonState& state, double x, double y, double z, bool crystalsLeft) {
    float closest = 10000.0F;
    std::int32_t closestIndex = 0;
    const Node here = makeNode(wire::common::mth::floor(x), wire::common::mth::floor(y), wire::common::mth::floor(z));
    // parity: with no crystals left the outer ring is off limits.
    const std::int32_t start = crystalsLeft ? 0 : kOuterRing;
    for (std::int32_t i = start; i < static_cast<std::int32_t>(kNodeCount); ++i) {
        const float squared = distanceSqr(state.nodes[static_cast<std::size_t>(i)], here);
        if (squared < closest) {
            closest = squared;
            closestIndex = i;
        }
    }
    return closestIndex;
}

std::optional<Path> findPath(DragonState& state, std::int32_t startIndex, std::int32_t endIndex,
                             std::optional<std::array<std::int32_t, 3>> finalNode, bool crystalsLeft) {
    // The 24 nodes, then the final one if there is one, as a pool the heap indexes into.
    std::vector<Node> pool{state.nodes.begin(), state.nodes.end()};
    for (Node& node : pool) {
        node.closed = false;
        node.f = 0.0F;
        node.g = 0.0F;
        node.h = 0.0F;
        node.cameFrom = -1;
        node.heapIdx = -1;
    }
    std::int32_t finalIndex = -1;
    if (finalNode.has_value()) {
        pool.push_back(makeNode((*finalNode)[0], (*finalNode)[1], (*finalNode)[2]));
        finalIndex = static_cast<std::int32_t>(pool.size() - 1);
    }
    const auto at = [&pool](std::int32_t index) -> Node& { return pool[static_cast<std::size_t>(index)]; };
    std::int32_t to = endIndex;
    at(startIndex).g = 0.0F;
    at(startIndex).h = distance(at(startIndex), at(to));
    at(startIndex).f = at(startIndex).h;
    NodeHeap open{pool};
    open.insert(startIndex);
    std::int32_t closest = startIndex;
    const std::int32_t minimum = crystalsLeft ? 0 : kOuterRing;

    const auto reconstruct = [&at](std::int32_t last) {
        Path path;
        for (std::int32_t index = last; index >= 0; index = at(index).cameFrom) {
            path.nodes.insert(path.nodes.begin(), {at(index).x, at(index).y, at(index).z});
        }
        return path;
    };

    while (!open.isEmpty()) {
        const std::int32_t current = open.pop();
        if (sameNode(at(current), at(to))) {
            if (finalIndex >= 0) {
                at(finalIndex).cameFrom = to;
                to = finalIndex;
            }
            return reconstruct(to);
        }
        if (distance(at(current), at(to)) < distance(at(closest), at(to))) {
            closest = current;
        }
        at(current).closed = true;
        // parity: the adjacency row is found by identity among the 24, which the final node never is.
        std::int32_t row = 0;
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(kNodeCount); ++i) {
            if (i == current) {
                row = i;
                break;
            }
        }
        for (std::int32_t i = minimum; i < static_cast<std::int32_t>(kNodeCount); ++i) {
            if ((state.adjacency[static_cast<std::size_t>(row)] & (1 << i)) <= 0) {
                continue;
            }
            Node& adjacent = at(i);
            if (adjacent.closed) {
                continue;
            }
            const float tentative = at(current).g + distance(at(current), adjacent);
            if (adjacent.heapIdx < 0 || tentative < adjacent.g) {
                adjacent.cameFrom = current;
                adjacent.g = tentative;
                adjacent.h = distance(adjacent, at(to));
                if (adjacent.heapIdx >= 0) {
                    open.changeCost(i, adjacent.g + adjacent.h);
                } else {
                    adjacent.f = adjacent.g + adjacent.h;
                    open.insert(i);
                }
            }
        }
    }
    if (closest == startIndex) {
        return std::nullopt;
    }
    if (finalIndex >= 0) {
        at(finalIndex).cameFrom = closest;
        closest = finalIndex;
    }
    return reconstruct(closest);
}

} // namespace basalt::server::runtime::entity::enderDragon
