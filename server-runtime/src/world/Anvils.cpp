#include <basalt/server/runtime/item/ItemComponents.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/world/Anvils.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

namespace basalt::server::runtime::world::anvils {
namespace {

namespace item = runtime::item;

/// parity: the `repair_items` of each tool material in the jar's item data.
constexpr auto kRepairMaterials =
    std::to_array<std::pair<std::string_view, std::string_view>>({{"wooden_", "oak_planks"},
                                                                  {"stone_", "cobblestone"},
                                                                  {"iron_", "iron_ingot"},
                                                                  {"golden_", "gold_ingot"},
                                                                  {"diamond_", "diamond"},
                                                                  {"netherite_", "netherite_ingot"},
                                                                  {"leather_", "leather"},
                                                                  {"chainmail_", "iron_ingot"}});

[[nodiscard]] std::optional<std::string> nameOf(const storage::StoredItemStack& stack) {
    const wire::nbt::NbtTag* value = stack.components.find(item::itemComponents::kCustomName);
    const std::string* name = value == nullptr ? nullptr : value->get<std::string>();
    return name == nullptr ? std::nullopt : std::optional{*name};
}

[[nodiscard]] storage::StoredItemStack withName(storage::StoredItemStack stack,
                                                const std::optional<std::string>& name) {
    if (name.has_value()) {
        stack.components.putString(item::itemComponents::kCustomName, *name);
    } else {
        static_cast<void>(stack.components.remove(item::itemComponents::kCustomName));
    }
    return stack;
}

[[nodiscard]] storage::StoredItemStack withDamage(storage::StoredItemStack stack, std::int32_t damage) {
    stack.components.putInt(item::itemComponents::kDamage, damage);
    return stack;
}

[[nodiscard]] storage::StoredItemStack withRepairCost(storage::StoredItemStack stack, std::int32_t cost) {
    stack.components.putInt(item::itemComponents::kRepairCost, cost);
    return stack;
}

} // namespace

std::int32_t repairCost(const storage::StoredItemStack& stack) {
    const wire::nbt::NbtTag* value = stack.components.find(item::itemComponents::kRepairCost);
    const std::int32_t* cost = value == nullptr ? nullptr : value->get<std::int32_t>();
    return cost == nullptr ? 0 : *cost;
}

std::int32_t increasedRepairCost(std::int32_t cost) {
    const std::int64_t doubled = (static_cast<std::int64_t>(cost) * 2) + 1;
    return static_cast<std::int32_t>(std::min<std::int64_t>(doubled, std::numeric_limits<std::int32_t>::max()));
}

bool isRepairMaterialFor(const storage::StoredItemStack& input, const storage::StoredItemStack& addition) {
    std::string_view plain = input.id;
    if (plain.starts_with("minecraft:")) {
        plain.remove_prefix(std::string_view{"minecraft:"}.size());
    }
    for (const auto& [prefix, material] : kRepairMaterials) {
        if (plain.starts_with(prefix)) {
            return addition.id == "minecraft:" + std::string{material};
        }
    }
    return false;
}

Result combine(const storage::StoredItemStack* input, const storage::StoredItemStack* addition,
               const std::optional<std::string>& newName, const IsRepairMaterial& isRepairMaterial,
               const MergeEnchantments& mergeEnchantments, const bool cumulativeCost) {
    if (input == nullptr) {
        return Result{};
    }
    const auto merge = [&mergeEnchantments](const storage::StoredItemStack& one,
                                            const storage::StoredItemStack& other) {
        return mergeEnchantments ? mergeEnchantments(one, other) : Merge{};
    };

    std::int32_t price = 0;
    const std::int32_t tax =
        cumulativeCost ? repairCost(*input) + (addition == nullptr ? 0 : repairCost(*addition)) : 0;
    storage::StoredItemStack result = *input;
    std::int32_t materials = 0;

    if (addition != nullptr) {
        const bool damageable = item::itemWear::isDamageable(*input);
        // parity: `usingBook`.
        const bool usingBook = addition->id == "minecraft:enchanted_book" ||
                               addition->components.find(item::itemComponents::kStoredEnchantments) != nullptr;
        if (damageable && isRepairMaterial && isRepairMaterial(*input, *addition)) {
            // parity: a quarter of the maximum per material, and one level for each one used.
            std::int32_t damage = item::itemWear::damage(result);
            const std::int32_t max = item::itemWear::maxDamage(result);
            if (damage <= 0) {
                return Result{};
            }
            std::int32_t used = 0;
            while (damage > 0 && used < addition->count) {
                damage = std::max(damage - std::min(damage, max / 4), 0);
                ++price;
                ++used;
            }
            materials = used;
            result = withDamage(std::move(result), damage);
        } else {
            // parity: anything but a book has to be the same damageable item, or the anvil refuses.
            if (!usingBook && (input->id != addition->id || !damageable)) {
                return Result{};
            }
            if (damageable && !usingBook) {
                // parity: the two durabilities added, plus twelve per cent of the maximum.
                const std::int32_t max = item::itemWear::maxDamage(*input);
                const std::int32_t left = max - item::itemWear::damage(*input);
                const std::int32_t extra = max - item::itemWear::damage(*addition) + (max * 12 / 100);
                const std::int32_t damage = std::max(max - (left + extra), 0);
                if (damage < item::itemWear::damage(*input)) {
                    result = withDamage(std::move(result), damage);
                    price += 2;
                }
            }
            const Merge merged = merge(result, *addition);
            if (merged.refused) {
                return Result{};
            }
            if (merged.components.has_value()) {
                result.components = *merged.components;
            }
            price += merged.cost;
            // parity: a stack of more than one is priced out of the anvil rather than refused, and.
            if (input->count > 1 && merged.cost > 0) {
                price = kTooExpensive;
            }
        }
    }

    // parity: the naming, which is one level whether it adds a name or takes one away.
    std::int32_t namingCost = 0;
    if (newName.has_value() && !newName->empty()) {
        if (nameOf(*input) != newName) {
            namingCost = 1;
            price += namingCost;
            result = withName(std::move(result), newName);
        }
    } else if (nameOf(*input).has_value()) {
        namingCost = 1;
        price += namingCost;
        result = withName(std::move(result), std::nullopt);
    }

    if (price <= 0) {
        return Result{};
    }
    const std::int64_t total = static_cast<std::int64_t>(tax) + price;
    auto cost = static_cast<std::int32_t>(std::clamp<std::int64_t>(total, 0, std::numeric_limits<std::int32_t>::max()));
    // parity: renaming alone is never refused; it is capped one under the limit instead.
    const bool onlyRenaming = namingCost == price && namingCost > 0;
    if (onlyRenaming && cost >= kTooExpensive) {
        cost = kTooExpensive - 1;
    }
    if (cost >= kTooExpensive) {
        return Result{.output = std::nullopt, .cost = cost, .onlyRenaming = onlyRenaming};
    }

    std::int32_t priorWork =
        cumulativeCost ? std::max(repairCost(result), addition == nullptr ? 0 : repairCost(*addition)) : 0;
    if (cumulativeCost && (namingCost != price || namingCost == 0)) {
        priorWork = increasedRepairCost(priorWork);
    }
    return Result{.output = withRepairCost(std::move(result), priorWork),
                  .cost = cost,
                  .materialCount = materials,
                  .onlyRenaming = onlyRenaming};
}

} // namespace basalt::server::runtime::world::anvils
