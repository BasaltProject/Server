#include <basalt/server/runtime/item/ItemComponents.hpp>
#include <basalt/server/runtime/world/AnvilEnchantments.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <utility>

namespace basalt::server::runtime::world::anvils {
namespace {

namespace itemComponents = runtime::item::itemComponents;

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

/// parity: `EnchantmentHelper.getComponentType`: a book keeps what it is carrying somewhere else,
/// because what is written in it is not on it.
[[nodiscard]] std::string_view componentFor(const storage::StoredItemStack& stack) {
    return stack.id == "minecraft:enchanted_book" ? itemComponents::kStoredEnchantments : itemComponents::kEnchantments;
}

/// What is written on a stack, by name. parity: `getEnchantmentsForCrafting`.
[[nodiscard]] std::map<Identifier, std::int32_t> writtenOn(const storage::StoredItemStack& stack) {
    std::map<Identifier, std::int32_t> levels;
    const NbtCompound* stored = stack.components.getCompound(componentFor(stack));
    if (stored == nullptr) {
        return levels;
    }
    for (const std::string& name : stored->names()) {
        const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(name);
        if (parsed.has_value()) {
            levels.emplace(*parsed, stored->getInt(name));
        }
    }
    return levels;
}

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

/// Everything one enchantment's own file says that an anvil asks about.
class Books {
public:
    explicit Books(EnchantmentBooks books) : books_(books) {}

    [[nodiscard]] const NbtCompound* definitionOf(const Identifier& id) const {
        return books_.enchantments == nullptr ? nullptr : books_.enchantments->definitionOf(id);
    }

    [[nodiscard]] std::int32_t anvilCost(const Identifier& id) const {
        const NbtCompound* definition = definitionOf(id);
        return definition == nullptr ? 0 : definition->getInt("anvil_cost");
    }

    [[nodiscard]] std::int32_t maxLevel(const Identifier& id) const {
        const NbtCompound* definition = definitionOf(id);
        const std::int32_t level = definition == nullptr ? 0 : definition->getInt("max_level");
        return std::max(level, 1);
    }

    /// parity: `Enchantment.canEnchant`, which is the `supported_items` of its own file.
    [[nodiscard]] bool goesOn(const Identifier& id, const storage::StoredItemStack& stack) const {
        const NbtCompound* definition = definitionOf(id);
        const std::expected<Identifier, wire::common::IdentifierError> item = Identifier::parse(stack.id);
        if (definition == nullptr || !item.has_value()) {
            return false;
        }
        return selects(textAt(*definition, "supported_items"), *item, books_.itemTags);
    }

    /// parity: `Enchantment.areCompatible`: each one's exclusive set is asked about the other.
    [[nodiscard]] bool compatible(const Identifier& one, const Identifier& other) const {
        return one != other && !excludes(one, other) && !excludes(other, one);
    }

private:
    [[nodiscard]] bool excludes(const Identifier& one, const Identifier& other) const {
        const NbtCompound* definition = definitionOf(one);
        return definition != nullptr && selects(textAt(*definition, "exclusive_set"), other, books_.enchantmentTags);
    }

    EnchantmentBooks books_;
};

} // namespace

MergeEnchantments enchantmentMerge(EnchantmentBooks books) {
    return [books = Books{books}](const storage::StoredItemStack& input,
                                  const storage::StoredItemStack& addition) -> Merge {
        std::map<Identifier, std::int32_t> levels = writtenOn(input);
        const std::map<Identifier, std::int32_t> offered = writtenOn(addition);
        // parity: half price out of a book, which is the only reason a book is worth anything.
        const bool usingBook = addition.id == "minecraft:enchanted_book" ||
                               addition.components.find(itemComponents::kStoredEnchantments) != nullptr;
        const bool ontoBook = input.id == "minecraft:enchanted_book";

        std::int32_t cost = 0;
        bool anyFits = false;
        bool anyDoesNot = false;
        for (const auto& [id, offeredLevel] : offered) {
            const auto held = levels.find(id);
            const std::int32_t current = held == levels.end() ? 0 : held->second;
            std::int32_t level = current == offeredLevel ? offeredLevel + 1 : std::max(offeredLevel, current);
            // parity: a book takes anything; anything else takes what its own file allows.
            bool fits = ontoBook || books.goesOn(id, input);
            for (const auto& [other, ignored] : levels) {
                if (other != id && !books.compatible(id, other)) {
                    fits = false;
                    ++cost;
                }
            }
            if (!fits) {
                anyDoesNot = true;
                continue;
            }
            anyFits = true;
            level = std::min(level, books.maxLevel(id));
            levels.insert_or_assign(id, level);
            const std::int32_t fee = usingBook ? std::max(1, books.anvilCost(id) / 2) : books.anvilCost(id);
            cost += fee * level;
        }
        if (anyDoesNot && !anyFits) {
            return Merge{.refused = true};
        }
        if (!anyFits) {
            return Merge{.cost = cost};
        }
        NbtCompound written;
        for (const auto& [id, level] : levels) {
            written.putInt(id.toString(), level);
        }
        NbtCompound components = input.components;
        components.set(componentFor(input), NbtTag::ofCompound(std::move(written)));
        return Merge{.components = std::move(components), .cost = cost};
    };
}

} // namespace basalt::server::runtime::world::anvils
