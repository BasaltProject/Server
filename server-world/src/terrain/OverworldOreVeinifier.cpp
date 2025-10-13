#include <basalt/server/world/terrain/OverworldOreVeinifier.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace basalt::server::world::terrain {
namespace {

struct VeinType {
    std::int32_t ore;
    std::int32_t rawOreBlock;
    std::int32_t filler;
    std::int32_t minY;
    std::int32_t maxY;
};

// parity: vanilla writes these as floats and widens them, so the widening is explicit here.
constexpr double kVeininessThreshold = static_cast<double>(0.4F);
constexpr double kMaxRichnessThreshold = static_cast<double>(0.6F);
constexpr double kSkipOreIfGapNoiseIsBelow = static_cast<double>(-0.3F);

[[nodiscard]] std::int32_t state(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"Missing vanilla ore-vein block state minecraft:" + std::string{path}};
    }
    return found->stateId;
}

const VeinType& copper() {
    static const VeinType type{.ore = state("copper_ore"),
                               .rawOreBlock = state("raw_copper_block"),
                               .filler = state("granite"),
                               .minY = 0,
                               .maxY = 50};
    return type;
}

const VeinType& iron() {
    static const VeinType type{.ore = state("deepslate_iron_ore"),
                               .rawOreBlock = state("raw_iron_block"),
                               .filler = state("tuff"),
                               .minY = -60,
                               .maxY = -8};
    return type;
}

} // namespace

OverworldOreVeinifier::OverworldOreVeinifier(const OverworldNoiseRouter& router, const std::int64_t seed,
                                             densityfunction::DensityCache* densityCache)
    : router_(&router), densityCache_(densityCache), positionalRandomFactory_([seed] {
          random::RandomSource source = random::RandomSource::xoroshiro(seed);
          random::RandomSource named = source.forkPositional().fromHashOf("minecraft:ore");
          return named.forkPositional();
      }()) {}

std::optional<std::int32_t> OverworldOreVeinifier::compute(const std::int32_t x, const std::int32_t y,
                                                           const std::int32_t z) const {
    // Both vein types are Y-bounded, and the checks below reject anything outside both bands whatever
    // the noise says. Testing Y first skips the veininess walk for five sixths of a column, and
    // reaches neither the density functions (pure) nor the positional random.
    const bool inIron = y >= iron().minY && y <= iron().maxY;
    const bool inCopper = y >= copper().minY && y <= copper().maxY;
    if (!inIron && !inCopper) {
        return std::nullopt;
    }

    densityfunction::FunctionContext context{x, y, z, densityCache_};
    const double veininess = router_->veinToggle->compute(context);
    const VeinType& type = veininess > 0.0 ? copper() : iron();
    const double ridgedVeininess = std::abs(veininess);
    const std::int32_t distanceFromTop = type.maxY - y;
    const std::int32_t distanceFromBottom = y - type.minY;
    if (distanceFromBottom < 0 || distanceFromTop < 0) {
        return std::nullopt;
    }

    const std::int32_t distanceFromEdge = std::min(distanceFromTop, distanceFromBottom);
    const double edgeRoundoff =
        wire::common::mth::clampedMap(static_cast<double>(distanceFromEdge), 0.0, 20.0, -0.2, 0.0);
    if (ridgedVeininess + edgeRoundoff < kVeininessThreshold) {
        return std::nullopt;
    }

    random::RandomSource random = positionalRandomFactory_.at(x, y, z);
    if (random.nextFloat() > 0.7F) {
        return std::nullopt;
    }
    if (router_->veinRidged->compute(context) >= 0.0) {
        return std::nullopt;
    }

    const double richness = wire::common::mth::clampedMap(ridgedVeininess, kVeininessThreshold, kMaxRichnessThreshold,
                                                          static_cast<double>(0.1F), static_cast<double>(0.3F));
    if (static_cast<double>(random.nextFloat()) < richness &&
        router_->veinGap->compute(context) > kSkipOreIfGapNoiseIsBelow) {
        return random.nextFloat() < 0.02F ? type.rawOreBlock : type.ore;
    }
    return type.filler;
}

} // namespace basalt::server::world::terrain
