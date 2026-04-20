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

using ItemTags = std::map<wire::common::Identifier, std::set<wire::common::Identifier>>;

[[nodiscard]] ItemTags itemTagsOf(const ResourceStack& resources);

/// parity: each registry's `DIRECT_CODEC`.
[[nodiscard]] std::optional<std::string> reloadableElementError(std::string_view registry, std::string_view text,
                                                                const ResourceStack& resources,
                                                                const ItemTags& itemTags);

} // namespace basalt::server::runtime::datapack
