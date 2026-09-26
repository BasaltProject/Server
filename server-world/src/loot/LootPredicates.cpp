#include <basalt/server/world/loot/LootPredicates.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
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

namespace {

/// parity: `EnchantmentsPredicate`, every requirement met by some enchantment on the item.
[[nodiscard]] bool meets(const std::vector<EnchantmentRequirement>& requirements,
                         const std::map<std::string, std::int32_t, std::less<>>& held, const TagLookup& tags) {
    return std::ranges::all_of(requirements, [&](const EnchantmentRequirement& requirement) {
        return std::ranges::any_of(held, [&](const auto& one) {
            const auto id = wire::common::Identifier::parse(one.first);
            return id.has_value() && matchesIdsOrTag(requirement.enchantments, "enchantment", *id, tags) &&
                   one.second >= requirement.min.value_or(0) &&
                   (!requirement.max.has_value() || one.second <= *requirement.max);
        });
    });
}

[[nodiscard]] std::map<std::string, std::int32_t, std::less<>> levelsIn(const wire::nbt::NbtCompound& components,
                                                                        std::string_view key) {
    std::map<std::string, std::int32_t, std::less<>> levels;
    if (const wire::nbt::NbtCompound* held = components.getCompound(key); held != nullptr) {
        for (const std::string& name : held->names()) {
            levels.emplace(name, held->getInt(name));
        }
    }
    return levels;
}

} // namespace

bool ItemPredicate::test(const std::optional<wire::common::Identifier>& item,
                         const std::map<std::string, std::int32_t, std::less<>>& itemEnchantments,
                         const TagLookup& tags) const {
    if (unsupported || !item.has_value() || !matchesIdsOrTag(items, "item", *item, tags) ||
        !storedEnchantments.empty() || !present.empty() || !exact.empty()) {
        return false;
    }
    return meets(enchantments, itemEnchantments, tags);
}

bool ItemPredicate::test(const wire::common::Identifier& item, const wire::nbt::NbtCompound& components,
                         const TagLookup& tags, std::int32_t stackCount) const {
    if (unsupported || !matchesIdsOrTag(items, "item", item, tags) ||
        (count.has_value() && !count->test(static_cast<double>(stackCount)))) {
        return false;
    }
    if (!std::ranges::all_of(present, [&components](const std::string& key) { return components.contains(key); })) {
        return false;
    }
    if (!std::ranges::all_of(exact, [&components](const auto& wanted) {
            const wire::nbt::NbtTag* held = components.find(wanted.first);
            return held != nullptr && *held == wanted.second;
        })) {
        return false;
    }
    return meets(enchantments, levelsIn(components, "minecraft:enchantments"), tags) &&
           meets(storedEnchantments, levelsIn(components, "minecraft:stored_enchantments"), tags);
}

bool DistancePredicate::test(const std::array<double, 3>& from, const std::array<double, 3>& to) const {
    // parity: the differences are floats before they are compared.
    const auto dx = static_cast<float>(from[0] - to[0]);
    const auto dy = static_cast<float>(from[1] - to[1]);
    const auto dz = static_cast<float>(from[2] - to[2]);
    return x.test(static_cast<double>(std::abs(dx))) && y.test(static_cast<double>(std::abs(dy))) &&
           z.test(static_cast<double>(std::abs(dz))) &&
           horizontal.testSquared(static_cast<double>(dx * dx + dz * dz)) &&
           absolute.testSquared(static_cast<double>(dx * dx + dy * dy + dz * dz));
}

namespace {

/// parity: `LocationPredicate.matches` at a point: the square it falls in, and the point itself.
[[nodiscard]] bool locatedAt(const LocationPredicate& predicate, const EntityFacts& entity,
                             const std::array<double, 3>& point, const TagLookup& tags) {
    if (!entity.placeAt) {
        return false;
    }
    PlaceFacts place =
        entity.placeAt(static_cast<std::int32_t>(std::floor(point[0])), static_cast<std::int32_t>(std::floor(point[1])),
                       static_cast<std::int32_t>(std::floor(point[2])));
    place.exact = point;
    return predicate.test(place, tags);
}

[[nodiscard]] bool flagOf(const EntityFacts& entity, std::string_view name) {
    const auto found = entity.flags.find(name);
    return found != entity.flags.end() && found->second;
}

} // namespace

bool EntityPredicate::test(const std::optional<EntityFacts>& entity, const TagLookup& tags,
                           const std::optional<std::array<double, 3>>& origin) const {
    if (unsupported || !entity.has_value() || !matchesIdsOrTag(types, "entity_type", entity->type, tags)) {
        return false;
    }
    for (const auto& [name, wanted] : flags) {
        if (flagOf(*entity, name) != wanted) {
            return false;
        }
    }
    if (distance.has_value() &&
        (!origin.has_value() || !entity->position.has_value() || !distance->test(*origin, *entity->position))) {
        return false;
    }
    if (location != nullptr &&
        (!entity->position.has_value() || !locatedAt(*location, *entity, *entity->position, tags))) {
        return false;
    }
    // parity: `SteppingOnPredicate`, only on the ground and at the centre of the square underfoot.
    if (steppingOn != nullptr) {
        if (!entity->position.has_value() || !flagOf(*entity, "is_on_ground")) {
            return false;
        }
        const std::array<double, 3> under{std::floor((*entity->position)[0]) + 0.5,
                                          std::floor((*entity->position)[1] - 1.0E-5) + 0.5,
                                          std::floor((*entity->position)[2]) + 0.5};
        if (!locatedAt(*steppingOn, *entity, under, tags)) {
            return false;
        }
    }
    for (const auto& [slot, wanted] : equipment) {
        const auto worn = entity->equipment.find(slot);
        const bool matched = worn != entity->equipment.end()
                                 ? wanted.test(worn->second.item, worn->second.components, tags, worn->second.count)
                                 : wanted.test(wire::common::Identifier::vanilla("air"), {}, tags, 0);
        if (!matched) {
            return false;
        }
    }
    for (const EffectRequirement& requirement : effects) {
        const auto found = entity->effects.find(requirement.effect);
        if (found == entity->effects.end() || !requirement.amplifier.test(found->second.amplifier) ||
            !requirement.duration.test(found->second.duration) ||
            (requirement.ambient.has_value() && *requirement.ambient != found->second.ambient) ||
            (requirement.visible.has_value() && *requirement.visible != found->second.visible)) {
            return false;
        }
    }
    if (blocksSetOnFire.has_value() &&
        (!entity->blocksSetOnFire.has_value() || !blocksSetOnFire->test(*entity->blocksSetOnFire))) {
        return false;
    }
    if (lookingAt != nullptr) {
        const std::shared_ptr<const EntityFacts> seen =
            entity->lookingAt != nullptr ? entity->lookingAt
                                         : (entity->findLookingAt ? entity->findLookingAt() : nullptr);
        if (seen == nullptr || !lookingAt->test(*seen, tags, origin)) {
            return false;
        }
    }
    if (passenger != nullptr && !std::ranges::any_of(entity->passengers, [this, &tags, &origin](const auto& rider) {
            return rider != nullptr && passenger->test(*rider, tags, origin);
        })) {
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
    for (const auto& [name, values] : componentsIn) {
        const auto found = entity->components.find(name);
        if (found == entity->components.end() || std::ranges::find(values, found->second) == values.end()) {
            return false;
        }
    }
    if (vehicle != nullptr) {
        if (entity->vehicleFacts != nullptr) {
            if (!vehicle->test(*entity->vehicleFacts, tags, origin)) {
                return false;
            }
        } else if (!entity->vehicle.has_value() ||
                   !vehicle->test(EntityFacts{.type = *entity->vehicle}, tags, origin)) {
            return false;
        }
    }
    if (!mainhand.has_value()) {
        return true;
    }
    // A stack with its components answers every part of the predicate; the bare id only some.
    if (const auto held = entity->equipment.find("mainhand"); held != entity->equipment.end()) {
        return mainhand->test(held->second.item, held->second.components, tags, held->second.count);
    }
    return mainhand->test(entity->mainhand, entity->mainhandEnchantments, tags);
}

bool LocationPredicate::test(const PlaceFacts& place, const TagLookup& tags) const {
    if (unsupported) {
        return false;
    }
    if (position.has_value() && !((*position)[0].test(place.exact[0]) && (*position)[1].test(place.exact[1]) &&
                                  (*position)[2].test(place.exact[2]))) {
        return false;
    }
    if (dimension.has_value() && place.dimension != dimension) {
        return false;
    }
    if (!biomes.empty() &&
        (!place.loaded || !place.biome.has_value() || !matchesIdsOrTag(biomes, "worldgen/biome", *place.biome, tags))) {
        return false;
    }
    if (!structures.empty() && (!place.loaded || !place.inStructure || !place.inStructure(structures))) {
        return false;
    }
    if (smokey.has_value() && (!place.loaded || *smokey != place.smokey)) {
        return false;
    }
    if (hasLight && (!place.loaded || (lightMin.has_value() && place.light < *lightMin) ||
                     (lightMax.has_value() && place.light > *lightMax))) {
        return false;
    }
    // parity: `BlockPredicate`, the block and every named property with the value asked for.
    if (hasBlock &&
        (!place.loaded || !place.block.has_value() ||
         (!blocks.empty() && !matchesIdsOrTag(blocks, "block", *place.block, tags)) || !state.test(place.properties))) {
        return false;
    }
    // parity: `FluidPredicate`, where an empty square is the empty fluid.
    if (hasFluid) {
        const wire::common::Identifier fluid = place.fluid.value_or(wire::common::Identifier::vanilla("empty"));
        const TagLookup fluidTags = [&place, &tags](std::string_view registry, const wire::common::Identifier& id,
                                                    std::string_view tag) {
            return place.fluidTagged ? place.fluidTagged(id, tag) : tags && tags(registry, id, tag);
        };
        if (!place.loaded || (!fluids.empty() && !matchesIdsOrTag(fluids, "fluid", fluid, fluidTags)) ||
            !fluidState.test(place.fluidProperties)) {
            return false;
        }
    }
    return !canSeeSky.has_value() || *canSeeSky == place.canSeeSky;
}

} // namespace basalt::server::world::loot
