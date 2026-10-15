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

namespace placedFeatures {

using Placement = std::function<void(Chunk&, random::RandomSource&, std::int32_t, std::int32_t, std::int32_t)>;

[[nodiscard]] const Placement* find(const wire::common::Identifier& id);

struct FeatureContext {
    std::int64_t seed = 0;
    const NoiseRouter* endRouter = nullptr;
    const surface::SurfaceRuleEnvironment* surface = nullptr;
    structure::TemplateLoader templates;
    std::function<std::int32_t(std::int32_t, std::int32_t, std::int32_t)> blockStateAt;
};

using ContextualPlacement =
    std::function<bool(Chunk&, random::RandomSource&, std::int32_t, std::int32_t, std::int32_t, const FeatureContext&)>;

[[nodiscard]] const ContextualPlacement* contextual(const wire::common::Identifier& id);

[[nodiscard]] const std::set<wire::common::Identifier>& contextualIds();

[[nodiscard]] const std::set<wire::common::Identifier>& ids();

} // namespace placedFeatures

} // namespace basalt::server::world::terrain::decoration
