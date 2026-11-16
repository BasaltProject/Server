#pragma once

#include <cstdint>

namespace basalt::server::runtime::world {

/// How much of a neighbour's shape update a block can react to, by state id.
namespace blockReactions {

enum class Kind : std::uint8_t {
    /// Anything may happen: run the whole cascade.
    kFull,
    /// No update, shape update or tick booking ever reacts to it.
    kInert,
    /// Only the liquid turn a shape update books.
    kLiquid,
    /// Only the fall a shape update books.
    kFalling,
};

[[nodiscard]] Kind kindOf(std::int32_t stateId);

[[nodiscard]] inline bool isInert(std::int32_t stateId) { return kindOf(stateId) == Kind::kInert; }

} // namespace blockReactions

} // namespace basalt::server::runtime::world
