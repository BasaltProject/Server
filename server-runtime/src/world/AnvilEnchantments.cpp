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

/// parity: `EnchantmentHelper.getComponentType`.
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

} // namespace

MergeEnchantments enchantmentMerge(EnchantmentBooks books) {
    return [books](const storage::StoredItemStack& input, const storage::StoredItemStack& addition) -> Merge {
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
