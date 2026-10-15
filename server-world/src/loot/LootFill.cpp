#include <basalt/server/registry/Items.hpp>
#include <basalt/server/world/loot/Loot.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace basalt::server::world::loot {
namespace {

/// parity: `Util.shuffle`, a Fisher-Yates walk from the back.
template <typename Value>
void shuffle(std::vector<Value>& values, random::RandomSource& random) {
    for (auto index = static_cast<std::int32_t>(values.size()); index > 1; --index) {
        const auto swapTo = static_cast<std::size_t>(random.nextInt(index));
        std::swap(values[static_cast<std::size_t>(index - 1)], values[swapTo]);
    }
}

/// parity: `Mth.nextInt`, inclusive and short-circuiting when the ends meet.
[[nodiscard]] std::int32_t between(random::RandomSource& random, std::int32_t least, std::int32_t most) {
    return least >= most ? least : random.nextInt(most - least + 1) + least;
}

/// parity: `LootTable.shuffleAndSplitItems`.
void shuffleAndSplit(std::vector<ItemStack>& result, std::size_t availableSlots, random::RandomSource& random) {
    std::vector<ItemStack> splittable;
    std::vector<ItemStack> kept;
    for (ItemStack& stack : result) {
        if (stack.count <= 0) {
            continue;
        }
        if (stack.count > 1) {
            splittable.push_back(std::move(stack));
        } else {
            kept.push_back(std::move(stack));
        }
    }
    result = std::move(kept);
    while (availableSlots > result.size() + splittable.size() && !splittable.empty()) {
        const auto picked =
            static_cast<std::size_t>(between(random, 0, static_cast<std::int32_t>(splittable.size()) - 1));
        ItemStack stack = std::move(splittable[picked]);
        splittable.erase(splittable.begin() + static_cast<std::ptrdiff_t>(picked));
        const std::int32_t removed = between(random, 1, stack.count / 2);
        ItemStack copy = stack;
        copy.count = removed;
        stack.count -= removed;
        if (stack.count > 1 && random.nextBoolean()) {
            splittable.push_back(std::move(stack));
        } else {
            result.push_back(std::move(stack));
        }
        if (copy.count > 1 && random.nextBoolean()) {
            splittable.push_back(std::move(copy));
        } else {
            result.push_back(std::move(copy));
        }
    }
    for (ItemStack& stack : splittable) {
        result.push_back(std::move(stack));
    }
    shuffle(result, random);
}

} // namespace

void LootTable::fill(std::vector<std::optional<ItemStack>>& slots, random::RandomSource& random,
                     const LootContext& context) const {
    std::vector<ItemStack> stacks;
    // parity: `createStackSplitter`, which cuts anything past the item's own stack size.
    generateRaw(
        random, context.luck, std::nullopt,
        [&stacks](ItemStack stack) {
            const std::int32_t most = server::registry::itemStacking::maxStackSize(stack.item);
            if (stack.count < most) {
                stacks.push_back(std::move(stack));
                return;
            }
            std::int32_t left = stack.count;
            while (left > 0) {
                ItemStack part = stack;
                part.count = std::min(most, left);
                left -= part.count;
                stacks.push_back(std::move(part));
            }
        },
        context);
    std::vector<std::size_t> available;
    for (std::size_t index = 0; index < slots.size(); ++index) {
        if (!slots[index].has_value() || slots[index]->count <= 0) {
            available.push_back(index);
        }
    }
    shuffle(available, random);
    shuffleAndSplit(stacks, available.size(), random);
    for (ItemStack& stack : stacks) {
        // parity: the jar warns and stops when the roll outgrows the container.
        if (available.empty()) {
            return;
        }
        slots[available.back()] = std::move(stack);
        available.pop_back();
    }
}

} // namespace basalt::server::world::loot
