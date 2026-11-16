#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::registry {

namespace itemProperties {
struct Repair {
    std::string target;
    bool isTag = false;
};

// parity: the `minecraft:rarity` of an item's default components; `common` unless the item sets one.
[[nodiscard]] std::string_view rarityOf(const wire::common::Identifier& item);

// parity: the `minecraft:repairable` of an item's default components, a tag or one item.
[[nodiscard]] std::optional<Repair> repairableOf(const wire::common::Identifier& item);
} // namespace itemProperties
} // namespace basalt::server::registry
