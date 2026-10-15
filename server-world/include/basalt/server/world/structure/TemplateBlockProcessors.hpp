#pragma once

#include <basalt/server/world/structure/StructureTemplate.hpp>

#include <vector>

namespace basalt::server::world::structure {

/// parity: the `templatesystem.StructureProcessor` implementations a template runs its blocks.
namespace templateBlockProcessors {

[[nodiscard]] TemplateBlockProcessor chain(std::vector<TemplateBlockProcessor> processors);

/// parity: `BlockIgnoreProcessor.STRUCTURE_BLOCK`, which drops only the structure void.
[[nodiscard]] TemplateBlockProcessor ignoreStructureBlock();

/// parity: `BlockIgnoreProcessor.STRUCTURE_AND_AIR`, which drops its air too.
[[nodiscard]] TemplateBlockProcessor ignoreStructureAndAir();

/// parity: `ProtectedBlockProcessor`.
[[nodiscard]] TemplateBlockProcessor protectFeatures();

/// parity: `LavaSubmergedBlockProcessor`.
[[nodiscard]] TemplateBlockProcessor lavaSubmerged();

/// parity: `BlackstoneReplaceProcessor`, the Nether variant's stone swap.
[[nodiscard]] TemplateBlockProcessor blackstoneReplace();

/// parity: `BlockAgeProcessor`.
[[nodiscard]] TemplateBlockProcessor age(float mossiness);

} // namespace templateBlockProcessors

} // namespace basalt::server::world::structure
