#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <stdexcept>

namespace basalt::server::world::terrain::surface {

random::RandomSource SurfaceRuleEnvironment::positionalRandom(std::int32_t, std::int32_t, std::int32_t) const {
    throw std::logic_error{"positional random is required by biome-specific surface extensions"};
}

std::int32_t SurfaceRuleEnvironment::clayBand(std::int32_t, std::int32_t, std::int32_t) const {
    throw std::logic_error{"clay bands are only implemented for the badlands biome"};
}

} // namespace basalt::server::world::terrain::surface
