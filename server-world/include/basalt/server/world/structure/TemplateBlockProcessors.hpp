#pragma once

#include <basalt/server/world/structure/StructureTemplate.hpp>

#include <vector>

namespace basalt::server::world::structure {

/// parity: the `templatesystem.StructureProcessor` implementations a template runs its blocks
/// through after the rule list. Any of them may drop a block outright, which a rule list cannot do.
namespace templateBlockProcessors {

/// Applies `processors` in order, stopping the moment one drops the block.
[[nodiscard]] TemplateBlockProcessor chain(std::vector<TemplateBlockProcessor> processors);

/// parity: `BlockIgnoreProcessor.STRUCTURE_BLOCK`, which drops only the structure void.
[[nodiscard]] TemplateBlockProcessor ignoreStructureBlock();

/// parity: `BlockIgnoreProcessor.STRUCTURE_AND_AIR`, which drops its air too.
[[nodiscard]] TemplateBlockProcessor ignoreStructureAndAir();

/// parity: `ProtectedBlockProcessor`. Whatever the world already put here and marked as not
/// replaceable by features stays: a structure never eats another one's blocks.
[[nodiscard]] TemplateBlockProcessor protectFeatures();

/// parity: `LavaSubmergedBlockProcessor`: anything not a full cube keeps the lava it sits in.
[[nodiscard]] TemplateBlockProcessor lavaSubmerged();

/// parity: `BlackstoneReplaceProcessor`, the Nether variant's stone swap.
[[nodiscard]] TemplateBlockProcessor blackstoneReplace();

/// parity: `BlockAgeProcessor`. The draw order is not the obvious one: building the replacement
/// arrays calls `getRandomFacingStairs` twice, so four draws happen before anything is chosen.
[[nodiscard]] TemplateBlockProcessor age(float mossiness);

} // namespace templateBlockProcessors

} // namespace basalt::server::world::structure
