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

} // namespace basalt::server::world::terrain::decoration::oreBlockRules
