#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <set>
#include <vector>

namespace basalt::server::world::terrain::decoration {

/// Every vanilla placed feature that can run from a chunk alone, by id, for `/place feature`. Names
/// differing only in placement modifiers (`oak`, `oak_checked`) run the same body, since the command
/// gives the position. Those needing more than a chunk (End spikes and chorus, fossils, the freeze
/// pass, the sulfur spring) answer to `contextual` instead.
namespace placedFeatures {

/// One feature at a world position, inside the chunk it is handed. The bodies read and write in
/// world coordinates through `decorationBlockStateId`, so the position is not masked to the chunk.
using Placement = std::function<void(Chunk&, random::RandomSource&, std::int32_t, std::int32_t, std::int32_t)>;

/// The placement for `id`, or null when nothing answers to that name.
[[nodiscard]] const Placement* find(const wire::common::Identifier& id);

/// What a feature needs beyond its chunk: the world's seed, its noise routers, its surface rules
/// and its structure templates.
///
/// A caller that has none of it gets a refusal rather than a half-placed feature.
struct FeatureContext {
    std::int64_t seed = 0;
    const NoiseRouter* endRouter = nullptr;
    const surface::SurfaceRuleEnvironment* surface = nullptr;
    structure::TemplateLoader templates;
    /// Loaded world-state view for configured features whose predicates cross chunk borders.
    std::function<std::int32_t(std::int32_t, std::int32_t, std::int32_t)> blockStateAt;
};

/// A feature that needs a `FeatureContext` as well as a position.
using ContextualPlacement =
    std::function<bool(Chunk&, random::RandomSource&, std::int32_t, std::int32_t, std::int32_t, const FeatureContext&)>;

/// The contextual placement for `id`, or null when nothing answers to that name.
[[nodiscard]] const ContextualPlacement* contextual(const wire::common::Identifier& id);

/// Every id that needs a `FeatureContext` and therefore cannot run from a chunk alone.
[[nodiscard]] const std::set<wire::common::Identifier>& contextualIds();

/// Every id this registry answers to, contextual ones included.
[[nodiscard]] const std::set<wire::common::Identifier>& ids();

} // namespace placedFeatures

} // namespace basalt::server::world::terrain::decoration
