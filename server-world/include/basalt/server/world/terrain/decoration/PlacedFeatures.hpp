#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <set>
#include <vector>

namespace basalt::server::world::terrain::decoration {

/// Every vanilla placed feature that can run from a chunk alone, by its own id: what
/// `/place feature` needs, since the decoration pipeline invokes a feature per step over a whole
/// chunk and never "this one, here".
///
/// Names that differ only in their placement modifiers (`oak` and `oak_checked`) run the same body,
/// because the modifiers decide where generation puts one and the command is told where instead.
///
/// The features that need more than a chunk (the End's spikes and chorus plants, fossils, the
/// freeze pass and the template-backed sulfur spring) are not here: they arrive with the structure
/// template layer.
namespace placedFeatures {

/// One feature at chunk-local `localX`/`localZ` and world `y`.
using Placement = std::function<void(Chunk&, random::RandomSource&, std::int32_t, std::int32_t, std::int32_t)>;

/// The placement for `id`, or null when nothing answers to that name.
[[nodiscard]] const Placement* find(const wire::common::Identifier& id);

/// Every id this registry answers to.
[[nodiscard]] const std::set<wire::common::Identifier>& ids();

} // namespace placedFeatures

} // namespace basalt::server::world::terrain::decoration
