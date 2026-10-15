#pragma once

#include <basalt/server/world/ChunkTypes.hpp>

#include <cstdint>
#include <functional>
#include <set>
#include <string>

namespace basalt::server::world::terrain::decoration::oreBlockRules {

/// parity: `OreFeature` re-tests its target predicate against whatever block is really there when it.
[[nodiscard]] bool canReplaceDeferred(GeneratedBlockWritePolicy policy, std::int32_t actualStateId, std::int32_t y);

/// parity: whether a chunk decorating later could have covered this block at all. Everything in the.
[[nodiscard]] bool survivesLaterDecoration(GeneratedBlockWritePolicy policy, std::int32_t writeStateId);

[[nodiscard]] GeneratedBlockWritePolicy policyFor(const std::set<std::string, std::less<>>& replaceable);

} // namespace basalt::server::world::terrain::decoration::oreBlockRules
