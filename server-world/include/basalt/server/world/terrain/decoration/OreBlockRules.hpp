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

[[nodiscard]] GeneratedBlockWritePolicy policyFor(const std::set<std::string, std::less<>>& replaceable);

/// parity: every overworld ore targets a subset of `base_stone_overworld`, so none of them can carve
/// through a dirt, gravel or clay blob. A write queued across a chunk edge that carries one of those
/// still wins over an ore the receiving chunk decorated on top, because in vanilla the blob was
/// already there and the ore had to go around it.
[[nodiscard]] bool outranksOreDecoration(std::int32_t writeStateId, std::int32_t currentStateId);

} // namespace basalt::server::world::terrain::decoration::oreBlockRules
