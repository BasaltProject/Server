#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/player/PlayerMovementStats.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <cmath>
#include <set>
#include <string>

namespace basalt::server::runtime::player::playerMovement {
namespace {

/// parity: the jar takes the square root as a float and rounds that, so the counted centimetres come
/// from float precision rather than double.
[[nodiscard]] std::int32_t centimetresOf(double dx, double dy, double dz) {
    const auto distance = static_cast<float>(std::sqrt((dx * dx) + (dy * dy) + (dz * dz)));
    return static_cast<std::int32_t>(std::lround(distance * 100.0F));
}

[[nodiscard]] std::int32_t horizontalOf(double dx, double dz) { return centimetresOf(dx, 0.0, dz); }

[[nodiscard]] bool didNotMove(double dx, double dy, double dz) { return dx == 0.0 && dy == 0.0 && dz == 0.0; }

[[nodiscard]] std::optional<Counted> counted(const std::string& key, std::int32_t centimetres) {
    return centimetres > 0 ? std::optional<Counted>{Counted{.key = key, .centimetres = centimetres}} : std::nullopt;
}

/// parity: `BlockTags.CLIMBABLE` plus the trapdoor rule, as `Entity.onClimbable` reads it.
[[nodiscard]] const std::set<std::string, std::less<>>& climbableBlocks() {
    static const std::set<std::string, std::less<>> kBlocks{"ladder",
                                                            "vine",
                                                            "scaffolding",
                                                            "twisting_vines",
                                                            "twisting_vines_plant",
                                                            "weeping_vines",
                                                            "weeping_vines_plant",
                                                            "cave_vines",
                                                            "cave_vines_plant"};
    return kBlocks;
}

} // namespace

bool isClimbable(const std::int32_t stateId) {
    const std::optional<registry::BlockState> block = registry::blocks::byStateId(stateId);
    return block.has_value() && climbableBlocks().contains(block->block.path());
}

std::optional<Counted> movementStat(double dx, double dy, double dz, const Facts& facts) {
    if (facts.riding || didNotMove(dx, dy, dz)) {
        return std::nullopt;
    }
    if (facts.swimming) {
        return counted(storage::stats::swimOneCm(), centimetresOf(dx, dy, dz));
    }
    if (facts.eyeInWater) {
        return counted(storage::stats::walkUnderWaterOneCm(), centimetresOf(dx, dy, dz));
    }
    if (facts.inWater) {
        return counted(storage::stats::walkOnWaterOneCm(), horizontalOf(dx, dz));
    }
    if (facts.climbing) {
        // parity: climbing counts only what was gained going up, and down counts for nothing.
        return dy > 0.0 ? counted(storage::stats::climbOneCm(), static_cast<std::int32_t>(std::lround(dy * 100.0)))
                        : std::nullopt;
    }
    if (facts.onGround) {
        const std::int32_t horizontal = horizontalOf(dx, dz);
        if (facts.sprinting) {
            return counted(storage::stats::sprintOneCm(), horizontal);
        }
        return counted(facts.crouching ? storage::stats::crouchOneCm() : storage::stats::walkOneCm(), horizontal);
    }
    if (facts.fallFlying) {
        return counted(storage::stats::custom("aviate_one_cm"), centimetresOf(dx, dy, dz));
    }
    // parity: the last branch is everything else in the air, and a step under a quarter of a block
    // counts for nothing: it is what keeps a player standing on a slab from flying on the spot.
    const std::int32_t horizontal = horizontalOf(dx, dz);
    return horizontal > kFlyingThreshold ? counted(storage::stats::flyOneCm(), horizontal) : std::nullopt;
}

} // namespace basalt::server::runtime::player::playerMovement
