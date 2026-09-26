#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/player/PlayerSpear.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <numbers>
#include <optional>

namespace basalt::server::runtime::player::spear {
namespace {

namespace mth = wire::common::mth;

constexpr double kEpsilon = 1.0E-7;

/// parity: `Item.Properties.spear`, the seconds turned into ticks the way it truncates them.
constexpr Spear make(std::string_view item, bool wood, float damageMultiplier, float delay, float dismountTime,
                     float dismountThreshold, float knockbackTime, float knockbackThreshold, float damageTime,
                     float damageThreshold) {
    return Spear{
        .item = item,
        .contactCooldownTicks = 10,
        .delayTicks = static_cast<std::int32_t>(delay * 20.0F),
        .dismount = {.maxDurationTicks = static_cast<std::int32_t>(dismountTime * 20.0F),
                     .minSpeed = dismountThreshold},
        .knockback = {.maxDurationTicks = static_cast<std::int32_t>(knockbackTime * 20.0F),
                      .minSpeed = knockbackThreshold},
        .damage = {.maxDurationTicks = static_cast<std::int32_t>(damageTime * 20.0F),
                   .minRelativeSpeed = damageThreshold},
        .damageMultiplier = damageMultiplier,
        .useSound = wood ? "item.spear_wood.use" : "item.spear.use",
        .hitSound = wood ? "item.spear_wood.hit" : "item.spear.hit",
        .attackSound = wood ? "item.spear_wood.attack" : "item.spear.attack",
    };
}

constexpr std::array<Spear, 7> kSpears{
    make("minecraft:wooden_spear", true, 0.7F, 0.75F, 5.0F, 14.0F, 10.0F, 5.1F, 15.0F, 4.6F),
    make("minecraft:stone_spear", false, 0.82F, 0.7F, 4.5F, 13.0F, 9.0F, 5.1F, 13.75F, 4.6F),
    make("minecraft:copper_spear", false, 0.82F, 0.65F, 4.0F, 12.0F, 8.25F, 5.1F, 12.5F, 4.6F),
    make("minecraft:iron_spear", false, 0.95F, 0.6F, 2.5F, 11.0F, 6.75F, 5.1F, 11.25F, 4.6F),
    make("minecraft:golden_spear", false, 0.7F, 0.7F, 3.5F, 13.0F, 8.5F, 5.1F, 13.75F, 4.6F),
    make("minecraft:diamond_spear", false, 1.075F, 0.5F, 3.0F, 10.0F, 6.5F, 5.1F, 10.0F, 4.6F),
    make("minecraft:netherite_spear", false, 1.2F, 0.4F, 2.5F, 9.0F, 5.5F, 5.1F, 8.75F, 4.6F),
};

using Vec = std::array<double, 3>;

[[nodiscard]] bool contains(const Box& box, const Vec& point) {
    return point[0] >= box.minX && point[0] < box.maxX && point[1] >= box.minY && point[1] < box.maxY &&
           point[2] >= box.minZ && point[2] < box.maxZ;
}

[[nodiscard]] Box inflate(const Box& box, double by) {
    return Box{box.minX - by, box.minY - by, box.minZ - by, box.maxX + by, box.maxY + by, box.maxZ + by};
}

[[nodiscard]] bool intersects(const Box& one, const Box& other) {
    return one.minX < other.maxX && one.maxX > other.minX && one.minY < other.maxY && one.maxY > other.minY &&
           one.minZ < other.maxZ && one.maxZ > other.minZ;
}

/// parity: `AABB.clip`: only faces the line enters through, strictly past its start.
[[nodiscard]] std::optional<Vec> clip(const Box& box, const Vec& from, const Vec& to) {
    const Vec delta{to[0] - from[0], to[1] - from[1], to[2] - from[2]};
    const std::array<double, 3> low{box.minX, box.minY, box.minZ};
    const std::array<double, 3> high{box.maxX, box.maxY, box.maxZ};
    double scale = 1.0;
    bool found = false;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (delta[axis] > -kEpsilon && delta[axis] < kEpsilon) {
            continue;
        }
        const double face = delta[axis] > 0.0 ? low[axis] : high[axis];
        const std::size_t b = (axis + 1) % 3;
        const std::size_t c = (axis + 2) % 3;
        const double s = (face - from[axis]) / delta[axis];
        const double pb = from[b] + (s * delta[b]);
        const double pc = from[c] + (s * delta[c]);
        if (0.0 < s && s < scale && low[b] - kEpsilon < pb && pb < high[b] + kEpsilon && low[c] - kEpsilon < pc &&
            pc < high[c] + kEpsilon) {
            scale = s;
            found = true;
        }
    }
    if (!found) {
        return std::nullopt;
    }
    return Vec{from[0] + (scale * delta[0]), from[1] + (scale * delta[1]), from[2] + (scale * delta[2])};
}

[[nodiscard]] double distanceSquared(const Vec& one, const Vec& other) {
    const double dx = one[0] - other[0];
    const double dy = one[1] - other[1];
    const double dz = one[2] - other[2];
    return (dx * dx) + (dy * dy) + (dz * dz);
}

} // namespace

bool Condition::test(std::int32_t ticksUsed, double attackerSpeed, double relativeSpeed, double entityFactor) const {
    return ticksUsed <= maxDurationTicks && attackerSpeed >= static_cast<double>(minSpeed) * entityFactor &&
           relativeSpeed >= static_cast<double>(minRelativeSpeed) * entityFactor;
}

const Spear* of(std::string_view itemId) {
    const auto found = std::ranges::find(kSpears, itemId, &Spear::item);
    return found == kSpears.end() ? nullptr : &*found;
}

bool tooSoon(std::int32_t ticksSinceSwing, double attackSpeed, std::int32_t tolerance) {
    if (attackSpeed <= 0.0) {
        return false;
    }
    // parity: `getCurrentItemAttackStrengthDelay`, a float.
    const auto delay = static_cast<float>(1.0 / attackSpeed * 20.0);
    return static_cast<float>(ticksSinceSwing + tolerance) / delay < kMinimumAttackCharge;
}

std::array<double, 3> lookOf(float yaw, float pitch) {
    const float x = pitch * (std::numbers::pi_v<float> / 180.0F);
    const float y = -yaw * (std::numbers::pi_v<float> / 180.0F);
    const float yCos = mth::cos(y);
    const float ySin = mth::sin(y);
    const float xCos = mth::cos(x);
    const float xSin = mth::sin(x);
    return {static_cast<double>(ySin * xCos), static_cast<double>(-xSin), static_cast<double>(yCos * xCos)};
}

std::vector<std::int32_t> hitsAlong(world::WorldState& world, const std::array<double, 3>& eye,
                                    const std::array<double, 3>& look, double movementAlongLook, bool creative,
                                    std::span<const Candidate> candidates) {
    const auto near = static_cast<double>(creative ? kRange.minCreativeReach : kRange.minReach);
    const double far =
        static_cast<double>(creative ? kRange.maxCreativeReach : kRange.maxReach) + std::max(0.0, movementAlongLook);
    const Vec from{eye[0] + (look[0] * near), eye[1] + (look[1] * near), eye[2] + (look[2] * near)};
    Vec to{eye[0] + (look[0] * far), eye[1] + (look[1] * far), eye[2] + (look[2] * far)};
    if (const std::optional<entity::BlockHit> block =
            entity::blockRay::cast(world, eye[0], eye[1], eye[2], to[0], to[1], to[2]);
        block.has_value()) {
        to = {block->x, block->y, block->z};
        if (distanceSquared(eye, to) < distanceSquared(eye, from)) {
            return {};
        }
    }
    const auto margin = static_cast<double>(kRange.hitboxMargin);
    // parity: `AABB.ofSize(from, margin...)` stretched to the far end, then a block all round.
    Box area{from[0] - (margin / 2.0), from[1] - (margin / 2.0), from[2] - (margin / 2.0),
             from[0] + (margin / 2.0), from[1] + (margin / 2.0), from[2] + (margin / 2.0)};
    const Vec stretch{to[0] - from[0], to[1] - from[1], to[2] - from[2]};
    (stretch[0] < 0.0 ? area.minX : area.maxX) += stretch[0];
    (stretch[1] < 0.0 ? area.minY : area.maxY) += stretch[1];
    (stretch[2] < 0.0 ? area.minZ : area.maxZ) += stretch[2];
    area = inflate(area, 1.0);
    std::vector<std::int32_t> hits;
    for (const Candidate& candidate : candidates) {
        if (!intersects(candidate.box, area)) {
            continue;
        }
        if (contains(candidate.box, from) || clip(candidate.box, from, to).has_value()) {
            hits.push_back(candidate.id);
            continue;
        }
        const std::optional<Vec> outside = clip(inflate(candidate.box, margin), from, to);
        if (!outside.has_value()) {
            continue;
        }
        // parity: the margin counts only if nothing solid stands between it and the box's centre.
        Vec towards{(candidate.box.minX + candidate.box.maxX) / 2.0, (candidate.box.minY + candidate.box.maxY) / 2.0,
                    (candidate.box.minZ + candidate.box.maxZ) / 2.0};
        if (const std::optional<entity::BlockHit> block = entity::blockRay::cast(
                world, (*outside)[0], (*outside)[1], (*outside)[2], towards[0], towards[1], towards[2]);
            block.has_value()) {
            towards = {block->x, block->y, block->z};
        }
        if (clip(candidate.box, *outside, towards).has_value()) {
            hits.push_back(candidate.id);
        }
    }
    return hits;
}

} // namespace basalt::server::runtime::player::spear
