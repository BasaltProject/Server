#include <basalt/server/world/loot/LootPredicates.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>

namespace basalt::server::world::loot {

bool matchesIdsOrTag(const std::vector<std::string>& wanted, std::string_view registry,
                     const wire::common::Identifier& id, const TagLookup& tags) {
    if (wanted.empty()) {
        return true;
    }
    if (wanted.size() == 1 && wanted.front().starts_with('#')) {
        return tags && tags(registry, id, std::string_view{wanted.front()}.substr(1));
    }
    const std::string name = id.toString();
    return std::ranges::find(wanted, name) != wanted.end();
}

namespace {

/// A state value as the property orders it: a whole number, or a flag with `false` first.
[[nodiscard]] std::optional<std::int64_t> ordinalOf(std::string_view value) {
    if (value == "false") {
        return 0;
    }
    if (value == "true") {
        return 1;
    }
    std::int64_t parsed = 0;
    const char* last = value.data() + value.size();
    const std::from_chars_result result = std::from_chars(value.data(), last, parsed);
    return result.ec == std::errc{} && result.ptr == last ? std::optional{parsed} : std::nullopt;
}

} // namespace

bool StateMatcher::test(const std::map<std::string, std::string>& state) const {
    return std::ranges::all_of(properties, [&state](const Property& property) {
        const auto found = state.find(property.name);
        if (found == state.end()) {
            return false;
        }
        if (property.exact.has_value()) {
            return found->second == *property.exact;
        }
        const std::optional<std::int64_t> value = ordinalOf(found->second);
        if (!value.has_value()) {
            return !property.min.has_value() && !property.max.has_value();
        }
        const auto bound = [](const std::optional<std::string>& text) { return ordinalOf(text.value_or("")); };
        if (property.min.has_value() && (!bound(property.min).has_value() || *value < *bound(property.min))) {
            return false;
        }
        return !property.max.has_value() || (bound(property.max).has_value() && *value <= *bound(property.max));
    });
}

bool ItemPredicate::test(const std::optional<wire::common::Identifier>& item,
                         const std::map<std::string, std::int32_t, std::less<>>& itemEnchantments,
                         const TagLookup& tags) const {
    if (unsupported || !item.has_value() || !matchesIdsOrTag(items, "item", *item, tags)) {
        return false;
    }
    // parity: `EnchantmentsPredicate`, every requirement met by some enchantment on the item.
    return std::ranges::all_of(enchantments, [&](const EnchantmentRequirement& requirement) {
        return std::ranges::any_of(itemEnchantments, [&](const auto& held) {
            const auto id = wire::common::Identifier::parse(held.first);
            return id.has_value() && matchesIdsOrTag(requirement.enchantments, "enchantment", *id, tags) &&
                   held.second >= requirement.min.value_or(0) &&
                   (!requirement.max.has_value() || held.second <= *requirement.max);
        });
    });
}

bool EntityPredicate::test(const std::optional<EntityFacts>& entity, const TagLookup& tags) const {
    if (unsupported || !entity.has_value() || !matchesIdsOrTag(types, "entity_type", entity->type, tags)) {
        return false;
    }
    if ((onFire.has_value() && *onFire != entity->onFire) || (baby.has_value() && *baby != entity->baby) ||
        (sheared.has_value() && *sheared != entity->sheared) ||
        (cubeSize.has_value() && *cubeSize != entity->cubeSize) ||
        (captain.has_value() && *captain != entity->captain)) {
        return false;
    }
    for (const auto& [name, value] : components) {
        const auto found = entity->components.find(name);
        if (found == entity->components.end() || found->second != value) {
            return false;
        }
    }
    if (vehicle != nullptr) {
        if (!entity->vehicle.has_value()) {
            return false;
        }
        EntityFacts ridden{.type = *entity->vehicle};
        if (!vehicle->test(ridden, tags)) {
            return false;
        }
    }
    return !mainhand.has_value() || mainhand->test(entity->mainhand, entity->mainhandEnchantments, tags);
}

bool LocationPredicate::test(const PlaceFacts& place, const TagLookup& tags) const {
    if (unsupported) {
        return false;
    }
    if (!biomes.empty() &&
        (!place.biome.has_value() || !matchesIdsOrTag(biomes, "worldgen/biome", *place.biome, tags))) {
        return false;
    }
    if (!hasBlock) {
        return true;
    }
    // parity: `BlockPredicate`, the block and every named property with the value asked for.
    if (!place.block.has_value() || !matchesIdsOrTag(blocks, "block", *place.block, tags)) {
        return false;
    }
    return state.test(place.properties);
}

} // namespace basalt::server::world::loot
