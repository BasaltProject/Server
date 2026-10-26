#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

namespace anvils {

/// parity: `AnvilMenu.MAX_COST`, the forty levels survival will not pay.
inline constexpr std::int32_t kTooExpensive = 40;

struct Result {
    std::optional<storage::StoredItemStack> output;
    std::int32_t cost = 0;
    std::int32_t materialCount = 0;
    bool onlyRenaming = false;
};

struct Merge {
    std::optional<wire::nbt::NbtCompound> components;
    std::int32_t cost = 0;
    /// parity: every enchantment offered went on something that cannot take it, which is the one.
    bool refused = false;
};

using IsRepairMaterial =
    std::function<bool(const storage::StoredItemStack& input, const storage::StoredItemStack& addition)>;
using MergeEnchantments =
    std::function<Merge(const storage::StoredItemStack& input, const storage::StoredItemStack& addition)>;

[[nodiscard]] Result combine(const storage::StoredItemStack* input, const storage::StoredItemStack* addition,
                             const std::optional<std::string>& newName = std::nullopt,
                             const IsRepairMaterial& isRepairMaterial = {},
                             const MergeEnchantments& mergeEnchantments = {}, bool cumulativeCost = true);

[[nodiscard]] std::int32_t increasedRepairCost(std::int32_t cost);

[[nodiscard]] std::int32_t repairCost(const storage::StoredItemStack& stack);

/// parity: `ItemStack.isValidRepairItem` for the vanilla tool and armour materials.
[[nodiscard]] bool isRepairMaterialFor(const storage::StoredItemStack& input, const storage::StoredItemStack& addition);

} // namespace anvils

} // namespace basalt::server::runtime::world
