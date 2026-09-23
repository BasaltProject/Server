#pragma once

#include <basalt/server/runtime/command/CommandTree.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::command {

/// A datapack element named by id or written in place. parity: `ResourceOrIdArgument`, which tries
/// an identifier first and only then an inline SNBT value.
struct ResourceOrInline {
    std::optional<wire::common::Identifier> id;
    /// The inline value, as the JSON the datapack readers take.
    std::optional<wire::common::JsonValue> inlineValue;
};

/// `minecraft:loot_modifier` in its 26.3 form, id or inline. The value is a `ResourceOrInline`.
[[nodiscard]] std::shared_ptr<const ArgumentType> lootModifierOrInlineArgument();

/// `minecraft:context_int_provider` and `minecraft:context_float_provider`, likewise.
[[nodiscard]] std::shared_ptr<const ArgumentType> contextIntProviderArgument();
[[nodiscard]] std::shared_ptr<const ArgumentType> contextFloatProviderArgument();

/// What a `minecraft:slot_source` names: a slot range, by its name and the vanilla slot ids in it.
struct SlotSourceValue {
    std::string name;
    std::vector<std::int32_t> slots;
};

/// `minecraft:slot_source`. Only the `SlotRanges` names are read; an inline or registered slot
/// source is refused by name. The value is a `SlotSourceValue`.
[[nodiscard]] std::shared_ptr<const ArgumentType> slotSourceArgument();

/// `minecraft:swing_animation`: `none`, `whack` or `stab`. The value is a `std::string`.
[[nodiscard]] std::shared_ptr<const ArgumentType> swingAnimationArgument();

/// parity: `SlotRanges`, every name and its slot ids, in registration order.
[[nodiscard]] const std::vector<SlotSourceValue>& slotRanges();

/// An NBT value as the JSON a datapack reader takes. A byte of 0 or 1 reads as a boolean, which is
/// how SNBT writes one.
[[nodiscard]] wire::common::JsonValue jsonOf(const wire::nbt::NbtTag& tag);

} // namespace basalt::server::runtime::command
