#pragma once

#include <basalt/server/runtime/command/CommandTree.hpp>

#include <cstdint>
#include <memory>
#include <set>

namespace basalt::server::runtime::command {

/// parity: `EntityAnchorArgument`.
enum class EntityAnchor : std::uint8_t { Feet, Eyes };

[[nodiscard]] std::shared_ptr<const ArgumentType> entityAnchorArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> swizzleArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> heightmapArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> itemSlotsArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> lootPredicateArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> lootTableArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> lootModifierArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> floatRangeArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> uuidArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> styleArgument();

} // namespace basalt::server::runtime::command
