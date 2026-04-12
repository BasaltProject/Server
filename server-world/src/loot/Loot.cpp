#include "LootComponentEdit.hpp"
#include "LootParts.hpp"

#include <basalt/server/registry/Items.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::world::loot {
namespace {

using wire::common::Identifier;

/// One weighed choice a pool can pick: a leaf entry, or one item or table of an expanded one, with
/// every entry whose modifier wraps it, outermost first.
struct Choice {
    const LootEntry* leaf = nullptr;
    std::optional<Identifier> item;
    const LootTable* table = nullptr;
    std::vector<const LootEntry*> chain;
};

/// Everything a roll carries down into nested tables. parity: `LootContext`, whose visited set is
/// what stops a table that names itself.
struct Roll {
    random::RandomSource& random;
    const LootContext& context;
    float luck = 0.0F;
    std::vector<const LootTable*>& visited;
};

void rollRaw(const LootTable& table, Roll& roll, const LootPool::Output& output);

[[nodiscard]] const LootTable* resolveNamed(const Identifier& id, const LootContext& context) {
    return context.tables ? context.tables(id) : nullptr;
}

[[nodiscard]] std::vector<const LootTable*> tablesOf(const LootEntry& entry, const LootContext& context) {
    std::vector<const LootTable*> found;
    for (const NestedTable& nested : entry.tables) {
        const LootTable* table = nested.inlined != nullptr ? nested.inlined.get()
                                 : nested.id.has_value()   ? resolveNamed(*nested.id, context)
                                                           : nullptr;
        if (table != nullptr) {
            found.push_back(table);
        }
    }
    if (entry.tableTag.has_value() && context.tagMembers) {
        for (const Identifier& id : context.tagMembers("loot_table", *entry.tableTag)) {
            if (const LootTable* table = resolveNamed(id, context); table != nullptr) {
                found.push_back(table);
            }
        }
    }
    return found;
}

[[nodiscard]] std::vector<Identifier> itemsOf(const LootEntry& entry, const LootContext& context) {
    if (!entry.tag.has_value()) {
        return entry.tagItems;
    }
    return context.tagMembers ? context.tagMembers("item", *entry.tag) : std::vector<Identifier>{};
}

/// parity: `LootPoolEntryContainer.expand`: the entry's own condition first, then the leaf's one
/// choice (or one per item or table when it expands) or the composite's children, `alternatives`
/// stopping at the first that expands, `sequence` at the first that does not, `group` never.
template <typename Found>
bool expand(const LootEntry& entry, Roll& roll, std::vector<const LootEntry*>& chain, const Found& found) {
    if (!entry.canRun(roll.random, roll.context)) {
        return false;
    }
    const bool wraps = !entry.functions.empty();
    if (wraps) {
        chain.push_back(&entry);
    }
    bool expanded = true;
    switch (entry.composite) {
    case LootEntry::Composite::None:
        if (entry.expand && (entry.tag.has_value() || !entry.tagItems.empty())) {
            for (const Identifier& item : itemsOf(entry, roll.context)) {
                found(Choice{.leaf = &entry, .item = item, .chain = chain});
            }
        } else if (entry.expand && (!entry.tables.empty() || entry.tableTag.has_value())) {
            for (const LootTable* table : tablesOf(entry, roll.context)) {
                found(Choice{.leaf = &entry, .table = table, .chain = chain});
            }
        } else {
            found(Choice{.leaf = &entry, .chain = chain});
        }
        break;
    case LootEntry::Composite::Alternatives:
        expanded = std::ranges::any_of(entry.children,
                                       [&](const LootEntry& child) { return expand(child, roll, chain, found); });
        break;
    case LootEntry::Composite::Sequence:
        expanded = std::ranges::all_of(entry.children,
                                       [&](const LootEntry& child) { return expand(child, roll, chain, found); });
        break;
    case LootEntry::Composite::Group:
        for (const LootEntry& child : entry.children) {
            static_cast<void>(expand(child, roll, chain, found));
        }
        break;
    }
    if (wraps) {
        chain.pop_back();
    }
    return expanded;
}

/// parity: `LootPoolEntry.createItemStack` through every modifier that wraps it, innermost first.
void createStacks(const Choice& choice, Roll& roll, const LootPool::Output& output) {
    const auto emit = [&](ItemStack stack) {
        for (auto wrapper = choice.chain.rbegin(); wrapper != choice.chain.rend(); ++wrapper) {
            for (const LootFunction& function : (*wrapper)->functions) {
                stack = function.apply(std::move(stack), roll.random, std::nullopt, roll.context);
            }
        }
        output(std::move(stack));
    };
    const LootEntry& leaf = *choice.leaf;
    if (choice.item.has_value()) {
        emit(ItemStack{.item = *choice.item, .count = 1});
        return;
    }
    if (choice.table != nullptr) {
        rollRaw(*choice.table, roll, emit);
        return;
    }
    if (leaf.item.has_value()) {
        emit(ItemStack{.item = *leaf.item, .count = 1});
        return;
    }
    // parity: `DynamicLoot`: whatever the block put in, which is several stacks and not one.
    if (leaf.dynamicName.has_value()) {
        if (const auto found = roll.context.dynamic.find(*leaf.dynamicName); found != roll.context.dynamic.end()) {
            for (const ItemStack& piece : found->second) {
                emit(piece);
            }
        }
        return;
    }
    // parity: `SlotLoot`, every non-empty stack the source reads.
    if (leaf.slotSource != nullptr) {
        for (ItemStack& stack : leaf.slotSource->provide(roll.context)) {
            if (stack.count > 0) {
                emit(std::move(stack));
            }
        }
        return;
    }
    // parity: `TagEntry` unexpanded, every item of the set at once.
    if (leaf.tag.has_value() || !leaf.tagItems.empty()) {
        for (const Identifier& item : itemsOf(leaf, roll.context)) {
            emit(ItemStack{.item = item, .count = 1});
        }
        return;
    }
    // parity: `NestedLootTable` unexpanded, every table rolled in turn on the same random.
    for (const LootTable* table : tablesOf(leaf, roll.context)) {
        rollRaw(*table, roll, emit);
    }
}

void addRandomItem(const LootPool& pool, Roll& roll, const LootPool::Output& output) {
    std::vector<Choice> expanded;
    std::vector<const LootEntry*> chain;
    const auto keep = [&expanded](Choice choice) { expanded.push_back(std::move(choice)); };
    for (const LootEntry& entry : pool.entries) {
        static_cast<void>(expand(entry, roll, chain, keep));
    }
    std::vector<Choice> valid;
    std::int32_t totalWeight = 0;
    for (Choice& choice : expanded) {
        const std::int32_t weight = choice.leaf->weightFor(roll.luck);
        if (weight > 0) {
            totalWeight += weight;
            valid.push_back(std::move(choice));
        }
    }
    if (totalWeight == 0 || valid.empty()) {
        return;
    }
    if (valid.size() == 1) {
        createStacks(valid.front(), roll, output);
        return;
    }
    std::int32_t index = roll.random.nextInt(totalWeight);
    for (const Choice& choice : valid) {
        index -= choice.leaf->weightFor(roll.luck);
        if (index < 0) {
            createStacks(choice, roll, output);
            return;
        }
    }
}

void rollPool(const LootPool& pool, Roll& roll, const LootPool::Output& output) {
    for (const LootCondition& condition : pool.conditions) {
        if (!condition.test(roll.random, roll.context)) {
            return;
        }
    }
    const LootPool::Output decorated = [&](ItemStack stack) {
        for (const LootFunction& function : pool.functions) {
            stack = function.apply(std::move(stack), roll.random, std::nullopt, roll.context);
        }
        output(std::move(stack));
    };
    const std::int32_t rolls = pool.rolls.get(roll.random, roll.context);
    const std::int32_t count =
        rolls +
        wire::common::mth::floor(static_cast<double>(pool.bonusRolls.get(roll.random, roll.context) * roll.luck));
    for (std::int32_t index = 0; index < count; ++index) {
        addRandomItem(pool, roll, decorated);
    }
}

void rollRaw(const LootTable& table, Roll& roll, const LootPool::Output& output) {
    // parity: `LootContext.pushVisitedElement`, a table already on the stack rolls nothing.
    if (std::ranges::find(roll.visited, &table) != roll.visited.end()) {
        return;
    }
    roll.visited.push_back(&table);
    const LootPool::Output decorated = [&](ItemStack stack) {
        for (const LootFunction& function : table.functions) {
            stack = function.apply(std::move(stack), roll.random, std::nullopt, roll.context);
        }
        output(std::move(stack));
    };
    for (const LootPool& pool : table.pools) {
        rollPool(pool, roll, decorated);
    }
    roll.visited.pop_back();
}

/// The context with the blast in it, copied only when the caller named a radius the context lacks.
template <typename Body>
void withRadius(const LootContext& context, std::optional<float> explosionRadius, const Body& body) {
    if (!explosionRadius.has_value() || context.explosionRadius == explosionRadius) {
        body(context);
        return;
    }
    LootContext blasted = context;
    blasted.explosionRadius = explosionRadius;
    body(blasted);
}

} // namespace

void rollContainerEntries(const std::vector<LootEntry>& entries, random::RandomSource& random,
                          const LootContext& context, const LootPool::Output& output) {
    std::vector<const LootTable*> visited;
    Roll roll{.random = random, .context = context, .luck = context.luck, .visited = visited};
    // parity: `LootTable.createStackSplitter`, a stack past its size handed on in full stacks.
    const LootPool::Output split = [&output](ItemStack stack) {
        const std::int32_t most = std::max(server::registry::itemStacking::maxStackSize(stack.item), 1);
        if (stack.count < most) {
            output(std::move(stack));
            return;
        }
        std::int32_t left = stack.count;
        while (left > 0) {
            ItemStack part = stack;
            part.count = std::min(most, left);
            left -= part.count;
            output(std::move(part));
        }
    };
    std::vector<const LootEntry*> chain;
    // parity: each choice makes its stacks the moment it is found, before the next condition rolls.
    const auto make = [&roll, &split](const Choice& choice) { createStacks(choice, roll, split); };
    for (const LootEntry& entry : entries) {
        static_cast<void>(expand(entry, roll, chain, make));
    }
}

std::int32_t LootEntry::weightFor(float luck) const {
    return std::max(wire::common::mth::floor(
                        static_cast<double>(static_cast<float>(weight) + (static_cast<float>(quality) * luck))),
                    0);
}

bool LootEntry::canRun(random::RandomSource& random, const LootContext& context) const {
    return std::ranges::all_of(conditions,
                               [&](const LootCondition& condition) { return condition.test(random, context); });
}

void LootPool::generate(random::RandomSource& random, float luck, std::optional<float> explosionRadius,
                        const Output& output, const LootContext& context) const {
    withRadius(context, explosionRadius, [&](const LootContext& rolled) {
        std::vector<const LootTable*> visited;
        Roll roll{.random = random, .context = rolled, .luck = luck, .visited = visited};
        rollPool(*this, roll, output);
    });
}

void LootTable::generateRaw(random::RandomSource& random, float luck, std::optional<float> explosionRadius,
                            const LootPool::Output& output, const LootContext& context) const {
    withRadius(context, explosionRadius, [&](const LootContext& rolled) {
        std::vector<const LootTable*> visited;
        Roll roll{.random = random, .context = rolled, .luck = luck, .visited = visited};
        rollRaw(*this, roll, output);
    });
}

std::vector<ItemStack> LootTable::generate(random::RandomSource& random, float luck,
                                           std::optional<float> explosionRadius, const LootContext& context) const {
    std::vector<ItemStack> output;
    generateRaw(
        random, luck, explosionRadius,
        [&output](ItemStack stack) {
            // An empty stack is no drop at all, and `createStackSplitter` cuts what outgrows the item.
            if (stack.count <= 0) {
                return;
            }
            const std::int32_t most = std::max(server::registry::itemStacking::maxStackSize(stack.item), 1);
            while (stack.count > most) {
                ItemStack part = stack;
                part.count = most;
                stack.count -= most;
                output.push_back(std::move(part));
            }
            output.push_back(std::move(stack));
        },
        context);
    return output;
}

} // namespace basalt::server::world::loot
