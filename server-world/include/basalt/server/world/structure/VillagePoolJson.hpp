#pragma once

#include <basalt/server/world/structure/StructureTemplatePool.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::structure {

/// Parses the real 26.2 `worldgen/template_pool` and `worldgen/processor_list` JSON, as villages and
/// bastions use them. Supported elements are single, legacy single, list, feature and empty;
/// processor lists retain rule processors and every rule predicate the 26.2 village and bastion
/// corpus uses.
namespace villagePoolJson {

/// Why a pool or processor list did not parse. A shape this does not understand is an error, not a
/// silently empty pool: a village whose pieces vanished would still generate, only wrong.
struct ParseError {
    std::string message;
};

/// Resolves a block tag name (`minecraft:doors`) to its member block state ids. Only called for
/// `tag_match` rule predicates and `block_rot`'s rottable set.
using TagResolver = std::function<std::optional<std::set<std::int32_t>>(std::string_view)>;

/// Resolves a named processor list reference (`minecraft:mossify_20_percent`) to its own JSON text.
/// Only called for the string form of a `processors` field.
using ProcessorListResolver = std::function<std::optional<std::string>(std::string_view)>;

[[nodiscard]] std::expected<StructureTemplatePool, ParseError>
parsePool(std::string_view json, const ProcessorListResolver& resolveProcessors, const TagResolver& resolveTag = {});

/// Parses a standalone `data/minecraft/worldgen/processor_list/<name>.json`.
[[nodiscard]] std::expected<StructureProcessorList, ParseError> parseProcessorList(std::string_view json,
                                                                                   const TagResolver& resolveTag = {});

} // namespace villagePoolJson

} // namespace basalt::server::world::structure
