#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/world/FarmlandTrample.hpp>

namespace basalt::server::runtime::world::farmlandTrample {

bool tramples(const config::GameplayRules& rules, const Faller& faller, random::RandomSource& random) {
    const config::GameplayRules::Blocks& blocks = rules.blocks;
    if (blocks.farmlandTrampleHeight >= 0.0F) {
        if (!(faller.fallDistance >= static_cast<double>(blocks.farmlandTrampleHeight))) {
            return false;
        }
    } else if (!(static_cast<double>(random.nextFloat()) < faller.fallDistance - 0.5)) {
        return false;
    }
    // parity: a body smaller than this (a chicken, a rabbit) is too light to break the soil.
    if (!(faller.width * faller.width * faller.height > 0.512F)) {
        return false;
    }
    if (blocks.farmlandDisableTrampling || (blocks.farmlandOnlyPlayersTrample && !faller.isPlayer)) {
        return false;
    }
    return !(blocks.farmlandFeatherFallDistanceAffectsTrampling && faller.featherFalling > 0 &&
             faller.featherFalling >= static_cast<std::int32_t>(faller.fallDistance));
}

} // namespace basalt::server::runtime::world::farmlandTrample
