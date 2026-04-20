#pragma once

#include <cstdint>
#include <set>
#include <string>

namespace basalt::server::world::terrain::decoration::tree {

using PathSet = std::set<std::string, std::less<>>;

namespace treeBlockRules {

[[nodiscard]] bool isLogState(std::int32_t stateId);

[[nodiscard]] bool isValidTreeState(std::int32_t stateId);

[[nodiscard]] bool canReplaceDeferred(std::int32_t actualStateId, std::int32_t generatedStateId);

[[nodiscard]] std::int32_t resolveDeferredState(std::int32_t actualStateId, std::int32_t generatedStateId);

/// parity: `BlockTags.LOGS`, expanded from the 26.2 server data pack.
[[nodiscard]] const PathSet& logBlockPaths();

} // namespace treeBlockRules

} // namespace basalt::server::world::terrain::decoration::tree
