#pragma once

#include <basalt/server/world/ChunkTypes.hpp>

#include <cstdint>
#include <functional>
#include <set>
#include <string>

namespace basalt::server::world::terrain::decoration::oreBlockRules {

/// parity: `OreFeature` re-tests its target predicate against whatever block is really there when it
/// writes. A deferred write across a chunk edge must too.
[[nodiscard]] bool canReplaceDeferred(GeneratedBlockWritePolicy policy, std::int32_t actualStateId);

/// parity: whether a chunk decorating later could have covered this block at all. Everything in the
/// steps after an ore targets `base_stone_overworld`, so a geode, a dirt vein or an ore survives
/// whatever the receiving chunk puts down; another stone variant could well have covered a variant.
[[nodiscard]] bool survivesLaterDecoration(GeneratedBlockWritePolicy policy, std::int32_t writeStateId);

[[nodiscard]] GeneratedBlockWritePolicy policyFor(const std::set<std::string, std::less<>>& replaceable);

} // namespace basalt::server::world::terrain::decoration::oreBlockRules
