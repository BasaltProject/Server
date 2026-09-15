#include <basalt/server/runtime/world/EnchantmentBooks.hpp>

#include <algorithm>
#include <expected>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

[[nodiscard]] const std::string* textAt(const NbtCompound& compound, const std::string_view name) {
    const NbtTag* value = compound.find(name);
    return value == nullptr ? nullptr : value->get<std::string>();
}

/// Whether a selector written as `#tag` or as a plain name covers this candidate.
[[nodiscard]] bool selects(const std::string* selector, const Identifier& candidate,
                           const std::map<Identifier, std::set<Identifier>>* tags) {
    if (selector == nullptr) {
        return false;
    }
    if (!selector->starts_with('#')) {
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(*selector);
        return id.has_value() && *id == candidate;
    }
    if (tags == nullptr) {
        return false;
    }
    const std::expected<Identifier, wire::common::IdentifierError> tag =
        Identifier::parse(std::string_view{*selector}.substr(1));
    if (!tag.has_value()) {
        return false;
    }
    const auto found = tags->find(*tag);
    return found != tags->end() && found->second.contains(candidate);
}

} // namespace

const NbtCompound* EnchantmentBooks::definitionOf(const Identifier& id) const {
    return enchantments == nullptr ? nullptr : enchantments->definitionOf(id);
}

std::int32_t EnchantmentBooks::anvilCost(const Identifier& id) const {
    const NbtCompound* definition = definitionOf(id);
    return definition == nullptr ? 0 : definition->getInt("anvil_cost");
}

std::int32_t EnchantmentBooks::maxLevel(const Identifier& id) const {
    const NbtCompound* definition = definitionOf(id);
    return std::max(definition == nullptr ? 0 : definition->getInt("max_level"), 1);
}

std::int32_t EnchantmentBooks::minCost(const Identifier& id, const std::int32_t level) const {
    const NbtCompound* definition = definitionOf(id);
    const NbtCompound* cost = definition == nullptr ? nullptr : definition->getCompound("min_cost");
    if (cost == nullptr) {
        return 0;
    }
    return cost->getInt("base") + (cost->getInt("per_level_above_first") * (level - 1));
}

std::int32_t EnchantmentBooks::maxCost(const Identifier& id, const std::int32_t level) const {
    const NbtCompound* definition = definitionOf(id);
    const NbtCompound* cost = definition == nullptr ? nullptr : definition->getCompound("max_cost");
    if (cost == nullptr) {
        return 0;
    }
    return cost->getInt("base") + (cost->getInt("per_level_above_first") * (level - 1));
}

std::int32_t EnchantmentBooks::weight(const Identifier& id) const {
    const NbtCompound* definition = definitionOf(id);
    return definition == nullptr ? 0 : definition->getInt("weight");
}

bool EnchantmentBooks::isPrimaryFor(const Identifier& id, const storage::StoredItemStack& stack) const {
    const NbtCompound* definition = definitionOf(id);
    const std::expected<Identifier, wire::common::IdentifierError> item = Identifier::parse(stack.id);
    if (definition == nullptr || !item.has_value()) {
        return false;
    }
    const std::string* primary = textAt(*definition, "primary_items");
    return selects(primary == nullptr ? textAt(*definition, "supported_items") : primary, *item, itemTags);
}

const std::set<Identifier>& EnchantmentBooks::inTag(const std::string_view tag) const {
    static const std::set<Identifier> empty;
    return enchantmentTags == nullptr ? empty : realEnchantmentTags::membersOf(*enchantmentTags, tag);
}

bool EnchantmentBooks::goesOn(const Identifier& id, const storage::StoredItemStack& stack) const {
    const NbtCompound* definition = definitionOf(id);
    const std::expected<Identifier, wire::common::IdentifierError> item = Identifier::parse(stack.id);
    if (definition == nullptr || !item.has_value()) {
        return false;
    }
    return selects(textAt(*definition, "supported_items"), *item, itemTags);
}

bool EnchantmentBooks::compatible(const Identifier& one, const Identifier& other) const {
    const auto excludes = [this](const Identifier& from, const Identifier& against) {
        const NbtCompound* definition = definitionOf(from);
        return definition != nullptr && selects(textAt(*definition, "exclusive_set"), against, enchantmentTags);
    };
    return one != other && !excludes(one, other) && !excludes(other, one);
}

bool EnchantmentBooks::isCurse(const Identifier& id) const {
    return realEnchantmentTags::contains(
        enchantmentTags == nullptr ? realEnchantmentTags::TagMembers{} : *enchantmentTags, "minecraft:curse", id);
}

} // namespace basalt::server::runtime::world
