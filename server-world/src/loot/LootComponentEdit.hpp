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
    /// `EquipmentSlotGroup` names, as written.
    std::vector<std::string> slots;
};

/// What the component editing functions carry. One struct rather than one class per function: the
/// kind is the tag, and each function reads the fields it owns.
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
    /// `toggles`, by component, in the order written.
    std::vector<std::pair<std::string, bool>> toggles;
    /// The custom data tag, the explosion or book fields set, the banner layers.
    wire::nbt::NbtCompound compound;
    /// The values a list operation brings: lore lines, pages, explosions, model strings and flags.
    std::vector<wire::nbt::NbtTag> values;
    ListOperation operation;
    /// `set_custom_model_data`, each list optional with its own operation.
    std::optional<std::pair<std::vector<FloatProvider>, ListOperation>> floats;
    std::optional<std::pair<std::vector<wire::nbt::NbtTag>, ListOperation>> flags;
    std::optional<std::pair<std::vector<wire::nbt::NbtTag>, ListOperation>> strings;
    std::optional<std::pair<std::vector<IntProvider>, ListOperation>> colors;
    /// `set_fireworks` and `set_firework_explosion` optionals; `set_banner_pattern.append`.
    bool hasOperation = false;
    std::optional<std::int32_t> flightDuration;
    bool append = false;
    /// `set_item`, `set_loot_table`.
    std::optional<wire::common::Identifier> id;
    std::int64_t seed = 0;
    EntityTarget entity = EntityTarget::This;
    std::vector<AttributeModifierSpec> modifiers;
    bool replace = true;
    /// `set_contents` and `modify_contents`: `container`, `bundle_contents` or `charged_projectiles`.
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

/// Runs one component editing function on a stack.
[[nodiscard]] ItemStack applyComponentEdit(const ComponentEdit& edit, ItemStack stack, random::RandomSource& random,
                                           const LootContext& context);

/// `set_contents` and `modify_contents`.
[[nodiscard]] ItemStack applyContainerEdit(const ComponentEdit& edit, ItemStack stack, random::RandomSource& random,
                                           const LootContext& context);

/// parity: `SetContainerContents.run`'s roll: every entry expanded, each choice making its stacks
/// as it is found, split to their stack size.
void rollContainerEntries(const std::vector<LootEntry>& entries, random::RandomSource& random,
                          const LootContext& context, const LootPool::Output& output);

/// The stacks a container component holds, empty slots of `minecraft:container` included.
[[nodiscard]] std::vector<ItemStack> containerItems(const ItemStack& holder, std::string_view component);

/// parity: `SlotRanges`, whether a slot range name exists.
[[nodiscard]] bool isSlotRange(std::string_view name);

} // namespace basalt::server::world::loot
