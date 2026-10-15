#pragma once

#include <basalt/server/world/loot/Loot.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::world::loot {

/// parity: `ListOperation`, how a function's values meet the list already on the item.
struct ListOperation {
    enum class Mode : std::uint8_t { ReplaceAll, ReplaceSection, Insert, Append };

    Mode mode = Mode::ReplaceAll;
    std::int32_t offset = 0;
    std::optional<std::int32_t> sectionSize;

    /// The list the operation leaves, or `original` when it would not fit `maxSize`.
    [[nodiscard]] std::vector<wire::nbt::NbtTag>
    apply(const std::vector<wire::nbt::NbtTag>& original, const std::vector<wire::nbt::NbtTag>& replacement,
          std::int32_t maxSize = std::numeric_limits<std::int32_t>::max()) const;
};

/// parity: `SetAttributesFunction.Modifier`.
struct AttributeModifierSpec {
    std::string id;
    std::string attribute;
    std::string operation;
    FloatProvider amount;
    std::vector<std::string> slots;
};

struct ComponentEdit {
    enum class Op : std::uint8_t {
        ToggleTooltips,
        SetCustomData,
        SetCustomModelData,
        SetLore,
        SetBookCover,
        SetWrittenBookPages,
        SetWritableBookPages,
        SetFireworkExplosion,
        SetFireworks,
        SetBannerPattern,
        SetItem,
        SetLootTable,
        FillPlayerHead,
        SetAttributes,
        SetContents,
        ModifyContents,
    };

    Op op = Op::SetItem;
    std::vector<std::pair<std::string, bool>> toggles;
    wire::nbt::NbtCompound compound;
    std::vector<wire::nbt::NbtTag> values;
    ListOperation operation;
    std::optional<std::pair<std::vector<FloatProvider>, ListOperation>> floats;
    std::optional<std::pair<std::vector<wire::nbt::NbtTag>, ListOperation>> flags;
    std::optional<std::pair<std::vector<wire::nbt::NbtTag>, ListOperation>> strings;
    std::optional<std::pair<std::vector<IntProvider>, ListOperation>> colors;
    bool hasOperation = false;
    std::optional<std::int32_t> flightDuration;
    bool append = false;
    std::optional<wire::common::Identifier> id;
    std::int64_t seed = 0;
    EntityTarget entity = EntityTarget::This;
    std::vector<AttributeModifierSpec> modifiers;
    bool replace = true;
    std::string component;
    std::vector<LootEntry> entries;
    std::vector<LootFunction> modifier;
};

/// parity: `SlotSource` and its six types.
struct SlotSource {
    enum class Kind : std::uint8_t { Group, Filtered, Limit, Range, Contents, Empty };

    Kind kind = Kind::Empty;
    std::vector<SlotSource> terms;
    std::shared_ptr<const SlotSource> child;
    ItemPredicate filter;
    std::int32_t limit = 0;
    std::string source = "container";
    std::string slots;
    std::string component;

    [[nodiscard]] std::vector<ItemStack> provide(const LootContext& context) const;
};

[[nodiscard]] ItemStack applyComponentEdit(const ComponentEdit& edit, ItemStack stack, random::RandomSource& random,
                                           const LootContext& context);

[[nodiscard]] ItemStack applyContainerEdit(const ComponentEdit& edit, ItemStack stack, random::RandomSource& random,
                                           const LootContext& context);

/// parity: `SetContainerContents.run`'s roll.
void rollContainerEntries(const std::vector<LootEntry>& entries, random::RandomSource& random,
                          const LootContext& context, const LootPool::Output& output);

[[nodiscard]] std::vector<ItemStack> containerItems(const ItemStack& holder, std::string_view component);

/// parity: `SlotRanges`, whether a slot range name exists.
[[nodiscard]] bool isSlotRange(std::string_view name);

} // namespace basalt::server::world::loot
