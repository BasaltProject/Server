#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::runtime::datapack {

class ResourceStack;

/// The item tags a recipe's ingredients may name, resolved once for a whole pass.
using ItemTags = std::map<wire::common::Identifier, std::set<wire::common::Identifier>>;

[[nodiscard]] ItemTags itemTagsOf(const ResourceStack& resources);

/// Why one element of a reloadable registry (`loot_table`, `recipe`, `advancement`...) does not decode,
/// or nothing when it does.
/// parity: each registry's `DIRECT_CODEC`. A type vanilla knows but this build cannot model yet passes;
/// an unknown type, a missing field or a dangling reference fails.
[[nodiscard]] std::optional<std::string> reloadableElementError(std::string_view registry, std::string_view text,
                                                                const ResourceStack& resources,
                                                                const ItemTags& itemTags);

} // namespace basalt::server::runtime::datapack
