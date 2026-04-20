#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/world/LootRegistries.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/WeaponDamage.hpp>

#include <algorithm>
#include <array>
#include <expected>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

namespace reg = ::basalt::server::registry;
namespace loot = ::basalt::server::world::loot;

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

constexpr std::array<std::string_view, 5> kRegistries{"enchantment", "instrument", "item", "fluid",
                                                      "worldgen/structure"};

[[nodiscard]] std::int32_t intOf(const NbtCompound& compound, std::string_view key, std::int32_t fallback) {
    const NbtTag* tag = compound.find(key);
    if (tag == nullptr) {
        return fallback;
    }
    if (const auto* whole = tag->get<std::int32_t>(); whole != nullptr) {
        return *whole;
    }
    if (const auto* real = tag->get<double>(); real != nullptr) {
        return static_cast<std::int32_t>(*real);
    }
    return fallback;
}

[[nodiscard]] loot::EnchantmentCost costOf(const NbtCompound& definition, std::string_view key) {
    const NbtCompound* cost = definition.getCompound(key);
    if (cost == nullptr) {
        return {};
    }
    return loot::EnchantmentCost{.base = intOf(*cost, "base", 0),
                                 .perLevelAboveFirst = intOf(*cost, "per_level_above_first", 0)};
}

} // namespace

LootRegistries::LootRegistries(const Jar& jar, const RealEnchantments& enchantments,
                               const wire::common::Logger& logger) {
    const auto selectorOf = [](const NbtCompound& definition, std::string_view key) {
        Selector selector;
        const NbtTag* tag = definition.find(key);
        if (tag == nullptr) {
            return selector;
        }
        selector.present = true;
        const auto parse = [&selector](std::string_view text) {
            const bool isTag = text.starts_with('#');
            const std::expected<Identifier, wire::common::IdentifierError> id =
                Identifier::parse(isTag ? text.substr(1) : text);
            if (!id.has_value()) {
                return;
            }
            if (isTag) {
                selector.tag = *id;
            } else {
                selector.ids.push_back(*id);
            }
        };
        if (const auto* text = tag->get<std::string>(); text != nullptr) {
            parse(*text);
        } else if (const auto* list = tag->get<NbtList>(); list != nullptr) {
            for (const NbtTag& each : *list) {
                if (const auto* one = each.get<std::string>(); one != nullptr) {
                    parse(*one);
                }
            }
        }
        return selector;
    };
    for (const auto& [id, definition] : enchantments.entries()) {
        Definition built{.entry = {.id = id,
                                   .weight = intOf(definition, "weight", 1),
                                   .maxLevel = intOf(definition, "max_level", 1),
                                   .minCost = costOf(definition, "min_cost"),
                                   .maxCost = costOf(definition, "max_cost")},
                         .supported = selectorOf(definition, "supported_items"),
                         .primary = selectorOf(definition, "primary_items"),
                         .exclusive = selectorOf(definition, "exclusive_set")};
        indices_.insert_or_assign(id, definitions_.size());
        definitions_.push_back(std::move(built));
    }

    for (const std::string_view registry : kRegistries) {
        const reg::tags::ElementExists exists = [this, registry](const Identifier& id) {
            if (registry == "item") {
                return reg::items::registry().byName(id) != nullptr;
            }
            return registry != "enchantment" || indices_.contains(id);
        };
        tags_.insert_or_assign(std::string{registry},
                               datapack::resolveTagsOrdered(jar.entriesUnder, jar.readText, registry, exists, logger));
    }

    enchanting_ = loot::Enchanting{
        .candidates = [this](const std::vector<std::string>& options) { return candidates(options); },
        .supports =
            [this](const Identifier& enchantment, const Identifier& item) {
                const Definition* found = definitionOf(enchantment);
                return found != nullptr && matches(found->supported, "item", item);
            },
        // parity: `isPrimaryItem`.
        .primary =
            [this](const Identifier& enchantment, const Identifier& item) {
                const Definition* found = definitionOf(enchantment);
                return found != nullptr &&
                       matches(found->primary.present ? found->primary : found->supported, "item", item);
            },
        // parity: `Enchantment.areCompatible`.
        .compatible =
            [this](const Identifier& first, const Identifier& second) {
                const Definition* one = definitionOf(first);
                const Definition* two = definitionOf(second);
                return first != second && (one == nullptr || !matches(one->exclusive, "enchantment", second)) &&
                       (two == nullptr || !matches(two->exclusive, "enchantment", first));
            },
        .enchantability = [](const Identifier& item) { return vanillaItemEnchantability::valueOf(item.toString()); },
    };
}

std::vector<Identifier> LootRegistries::tagMembers(std::string_view registry, const Identifier& tag) const {
    const auto found = tags_.find(registry);
    if (found == tags_.end()) {
        return {};
    }
    const auto members = found->second.find(tag);
    return members == found->second.end() ? std::vector<Identifier>{} : members->second;
}

bool LootRegistries::matches(const Selector& selector, std::string_view registry, const Identifier& id) const {
    if (std::ranges::find(selector.ids, id) != selector.ids.end()) {
        return true;
    }
    if (!selector.tag.has_value()) {
        return false;
    }
    const std::vector<Identifier> members = tagMembers(registry, *selector.tag);
    return std::ranges::find(members, id) != members.end();
}

const LootRegistries::Definition* LootRegistries::definitionOf(const Identifier& id) const {
    const auto found = indices_.find(id);
    return found == indices_.end() ? nullptr : &definitions_[found->second];
}

std::vector<const loot::EnchantmentEntry*> LootRegistries::candidates(const std::vector<std::string>& options) const {
    std::vector<const loot::EnchantmentEntry*> found;
    // parity: no options is the whole registry, in registry order.
    if (options.empty()) {
        for (const Definition& definition : definitions_) {
            found.push_back(&definition.entry);
        }
        return found;
    }
    for (const std::string& option : options) {
        const bool isTag = option.starts_with('#');
        const std::expected<Identifier, wire::common::IdentifierError> id =
            Identifier::parse(isTag ? std::string_view{option}.substr(1) : std::string_view{option});
        if (!id.has_value()) {
            continue;
        }
        const std::vector<Identifier> ids = isTag ? tagMembers("enchantment", *id) : std::vector<Identifier>{*id};
        for (const Identifier& each : ids) {
            if (const Definition* definition = definitionOf(each); definition != nullptr) {
                found.push_back(&definition->entry);
            }
        }
    }
    return found;
}

} // namespace basalt::server::runtime::world
