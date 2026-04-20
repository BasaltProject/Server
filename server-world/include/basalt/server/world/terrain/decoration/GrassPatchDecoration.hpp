#pragma once

#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

namespace grassPatchDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

void decorateFeature(std::int32_t index, Chunk& chunk, std::int64_t seed,
                     const surface::SurfaceRuleEnvironment& environment,
                     ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

[[nodiscard]] const std::map<std::string, std::int32_t, std::less<>>& placedGrassIds();

bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

} // namespace grassPatchDecoration

} // namespace basalt::server::world::terrain::decoration
