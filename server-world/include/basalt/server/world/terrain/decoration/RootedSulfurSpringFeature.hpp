#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::world::terrain::decoration {

/// parity: the 26.2 `rooted_sulfur_spring` root-system feature and its nested sulfur-spring
/// selector.
namespace rootedSulfurSpringFeature {

/// Reads a world block, so the root column can look past the chunk it starts in.
using BlockStateAt = std::function<std::int32_t(const DecorationPos&)>;

bool place(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
           const structure::TemplateLoader& templates, const BlockStateAt& blockStateAt);

} // namespace rootedSulfurSpringFeature

} // namespace basalt::server::world::terrain::decoration
