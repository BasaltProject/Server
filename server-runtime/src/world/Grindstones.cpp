#include <basalt/server/runtime/item/ItemComponents.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/world/Anvils.hpp>
#include <basalt/server/runtime/world/Grindstones.hpp>

#include <algorithm>
#include <expected>
#include <map>
#include <string>
#include <utility>

namespace basalt::server::runtime::world::grindstones {
namespace {

namespace item = runtime::item;

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

/// parity: `EnchantmentHelper.getComponentType`: a book keeps what it carries somewhere else.
[[nodiscard]] std::string_view componentFor(const storage::StoredItemStack& stack) {
    return stack.id == "minecraft:enchanted_book" ? item::itemComponents::kStoredEnchantments
                                                  : item::itemComponents::kEnchantments;
}

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

[[nodiscard]] bool anyEnchantments(const storage::StoredItemStack& stack) { return !writtenOn(stack).empty(); }

void write(storage::StoredItemStack& stack, const std::map<Identifier, std::int32_t>& levels) {
    if (levels.empty()) {
        static_cast<void>(stack.components.remove(componentFor(stack)));
        return;
    }
    NbtCompound written;
    for (const auto& [id, level] : levels) {
        written.putInt(id.toString(), level);
    }
    stack.components.set(componentFor(stack), NbtTag::ofCompound(std::move(written)));
}

/// parity: `removeNonCursesFrom`: everything but the curses comes off, an enchanted book with
/// nothing left on it goes back to being a book, and the prior work is rebuilt from what is left.
[[nodiscard]] storage::StoredItemStack stripped(storage::StoredItemStack stack, const IsCurse& isCurse) {
    std::map<Identifier, std::int32_t> kept;
    for (const auto& [id, level] : writtenOn(stack)) {
        if (isCurse && isCurse(id)) {
            kept.emplace(id, level);
        }
    }
    write(stack, kept);
    if (stack.id == "minecraft:enchanted_book" && kept.empty()) {
        stack.id = "minecraft:book";
    }
    std::int32_t repairCost = 0;
    for (std::size_t at = 0; at < kept.size(); ++at) {
        repairCost = anvils::increasedRepairCost(repairCost);
    }
    stack.components.putInt(item::itemComponents::kRepairCost, repairCost);
    return stack;
}

/// parity: `mergeItems`, the two durabilities added plus five per cent of the maximum.
[[nodiscard]] std::optional<storage::StoredItemStack>
merged(const storage::StoredItemStack& input, const storage::StoredItemStack& addition, const IsCurse& isCurse) {
    if (input.id != addition.id) {
        return std::nullopt;
    }
    const bool damageable = item::itemWear::isDamageable(input);
    storage::StoredItemStack out = input;
    if (damageable) {
        const std::int32_t max = std::max(item::itemWear::maxDamage(input), item::itemWear::maxDamage(addition));
        const std::int32_t left =
            (max - item::itemWear::damage(input)) + (max - item::itemWear::damage(addition)) + (max * 5 / 100);
        out.components.putInt(item::itemComponents::kDamage, std::max(max - left, 0));
    } else {
        // parity: two of something that does not wear only merge when they are the same stack to
        // the last component, and then they come out as two.
        if (input.components != addition.components) {
            return std::nullopt;
        }
        out.count = 2;
    }
    // parity: `mergeEnchantsFrom`, which takes the higher of the two of everything but a curse the
    // target already carries.
    std::map<Identifier, std::int32_t> levels = writtenOn(out);
    for (const auto& [id, level] : writtenOn(addition)) {
        const auto held = levels.find(id);
        const bool curse = isCurse && isCurse(id);
        if (curse && held != levels.end()) {
            continue;
        }
        const std::int32_t current = held == levels.end() ? 0 : held->second;
        levels.insert_or_assign(id, current == level ? level + 1 : std::max(level, current));
    }
    write(out, levels);
    return stripped(std::move(out), isCurse);
}

/// parity: `getExperienceFromItem`: what everything but the curses on it costs at a table.
[[nodiscard]] std::int32_t worthOf(const storage::StoredItemStack& stack, const IsCurse& isCurse,
                                   const MinCost& minCost) {
    if (!minCost) {
        return 0;
    }
    std::int32_t worth = 0;
    for (const auto& [id, level] : writtenOn(stack)) {
        if (!isCurse || !isCurse(id)) {
            worth += minCost(id, level);
        }
    }
    return worth;
}

} // namespace

Result combine(const storage::StoredItemStack* input, const storage::StoredItemStack* addition, const IsCurse& isCurse,
               const MinCost& minCost) {
    if (input == nullptr && addition == nullptr) {
        return Result{};
    }
    // parity: a stack of more than one goes in but makes nothing.
    if ((input != nullptr && input->count > 1) || (addition != nullptr && addition->count > 1)) {
        return Result{};
    }
    const std::int32_t worth = (input == nullptr ? 0 : worthOf(*input, isCurse, minCost)) +
                               (addition == nullptr ? 0 : worthOf(*addition, isCurse, minCost));
    if (input == nullptr || addition == nullptr) {
        const storage::StoredItemStack& alone = input == nullptr ? *addition : *input;
        if (!anyEnchantments(alone)) {
            return Result{};
        }
        return Result{.output = stripped(alone, isCurse), .experience = worth};
    }
    std::optional<storage::StoredItemStack> out = merged(*input, *addition, isCurse);
    if (!out.has_value()) {
        return Result{};
    }
    return Result{.output = std::move(out), .experience = worth};
}

std::int32_t experienceFor(const std::int32_t worth, const std::int32_t roll) {
    if (worth <= 0) {
        return 0;
    }
    const std::int32_t half = (worth + 1) / 2;
    return half + std::clamp(roll, 0, std::max(half - 1, 0));
}

} // namespace basalt::server::runtime::world::grindstones
