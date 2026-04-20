#pragma once

#include <basalt/server/runtime/command/CommandTree.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::command {

/// parity: `ResourceOrIdArgument`.
struct ResourceOrInline {
    std::optional<wire::common::Identifier> id;
    std::optional<wire::common::JsonValue> inlineValue;
};

[[nodiscard]] std::shared_ptr<const ArgumentType> lootModifierOrInlineArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> lootPredicateOrInlineArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> contextIntProviderArgument();
[[nodiscard]] std::shared_ptr<const ArgumentType> contextFloatProviderArgument();

struct SlotSourceValue {
    std::string name;
    std::vector<std::int32_t> slots;
};

[[nodiscard]] std::shared_ptr<const ArgumentType> slotSourceArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> swingAnimationArgument();

/// parity: `SlotRanges`, every name and its slot ids, in registration order.
[[nodiscard]] const std::vector<SlotSourceValue>& slotRanges();

[[nodiscard]] wire::common::JsonValue jsonOf(const wire::nbt::NbtTag& tag);

} // namespace basalt::server::runtime::command
