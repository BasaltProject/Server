#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

/// What an anvil makes of two items, and what it charges.
///
/// parity: `AnvilMenu.createResult`, in its order: the repair cost of both items is the tax, the work
/// itself is the price, renaming is one level on top, and the whole thing is refused at forty. The
/// prior-work cost doubles and adds one every time, which is what makes an item "too expensive".
///
/// Merging enchantments needs each enchantment's own `anvil_cost` and its exclusive set, both of
/// which come from the jar's enchantment files, so it arrives as a callback.
namespace anvils {

/// parity: `AnvilMenu.MAX_COST`, the forty levels survival will not pay.
inline constexpr std::int32_t kTooExpensive = 40;

/// What an anvil would produce, and what it asks for it.
struct Result {
    std::optional<storage::StoredItemStack> output;
    std::int32_t cost = 0;
    std::int32_t materialCount = 0;
    /// parity: `onlyRenaming`. The naming was the whole of the work, which is what leaves the prior
    /// work alone, lifts the forty-level wall, and leaves whatever is in the second slot there.
    bool onlyRenaming = false;
};

/// What merging two stacks' enchantments comes to: the components to keep, and what they cost.
struct Merge {
    std::optional<wire::nbt::NbtCompound> components;
    std::int32_t cost = 0;
    /// parity: every enchantment offered went on something that cannot take it, which is the one
    /// way a merge refuses the pair outright rather than just charging nothing for it.
    bool refused = false;
};

using IsRepairMaterial =
    std::function<bool(const storage::StoredItemStack& input, const storage::StoredItemStack& addition)>;
using MergeEnchantments =
    std::function<Merge(const storage::StoredItemStack& input, const storage::StoredItemStack& addition)>;

/// parity: `AnvilMenu.createResult`, minus the enchantment merge, which is handed in so this stays a
/// pure function of the two stacks.
[[nodiscard]] Result combine(const storage::StoredItemStack* input, const storage::StoredItemStack* addition,
                             const std::optional<std::string>& newName = std::nullopt,
                             const IsRepairMaterial& isRepairMaterial = {},
                             const MergeEnchantments& mergeEnchantments = {});

/// parity: `AnvilMenu.calculateIncreasedRepairCost`: it doubles and adds one, every time.
[[nodiscard]] std::int32_t increasedRepairCost(std::int32_t cost);

[[nodiscard]] std::int32_t repairCost(const storage::StoredItemStack& stack);

/// parity: `ItemStack.isValidRepairItem` for the vanilla tool and armour materials.
[[nodiscard]] bool isRepairMaterialFor(const storage::StoredItemStack& input, const storage::StoredItemStack& addition);

} // namespace anvils

} // namespace basalt::server::runtime::world
