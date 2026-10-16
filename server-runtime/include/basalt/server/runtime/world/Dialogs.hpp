#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world::dialogs {

/// parity: `ClickEvent.Action`.
enum class Press : std::uint8_t {
    None,
    RunCommand,
    ShowText,
    OpenDialog,
    RunTemplate,
};

/// parity: `InputControlTypes`.
enum class InputKind : std::uint8_t {
    Text,
    Boolean,
    NumberRange,
    SingleOption,
};

struct Option {
    std::string id;
    std::optional<wire::text::Component> display;
    bool initial = false;
};

/// parity: `Input` and the four `InputControl` records, flattened into the fields each one has.
struct Input {
    std::string key;
    InputKind kind = InputKind::Text;
    wire::text::Component label;
    bool labelVisible = true;
    std::string initialText;
    std::int32_t maxLength = 32;
    bool multiline = false;
    bool initialBool = false;
    std::string onTrue = "true";
    std::string onFalse = "false";
    float start = 0.0F;
    float end = 0.0F;
    std::optional<float> initialNumber;
    std::optional<float> step;
    std::vector<Option> options;
};

struct Dialog;

struct Button {
    wire::text::Component label;
    Press press = Press::None;
    std::string value;
    /// Shared because a button may point at a registry entry that several buttons also point at.
    std::shared_ptr<const Dialog> next;
};

struct Dialog {
    wire::text::Component title;
    std::vector<wire::text::Component> body;
    std::vector<Button> buttons;
    /// parity: `CommonDialogData.inputs`.
    std::vector<Input> inputs;
};

/// parity: `ParsedTemplate.instantiate`, every `$(name)` replaced by what that input holds, or nothing.
[[nodiscard]] std::string instantiate(std::string_view text, const std::map<std::string, std::string>& values);

/// parity: `NumberRangeInput.RangeInfo.computeScaledValue` for a slider at `sliderValue`, between 0 and 1.
[[nodiscard]] float scaledNumber(const Input& input, float sliderValue);

/// parity: `NumberRangeInput.RangeInfo.initialSliderValue`.
[[nodiscard]] float initialSlider(const Input& input);

/// parity: `InputControlHandlers` text substitution: whole numbers print without a point.
[[nodiscard]] std::string numberText(float value);

/// parity: `StringTag.escapeWithoutQuotes`.
[[nodiscard]] std::string escapeText(std::string_view text);

using ByName = std::function<const wire::nbt::NbtCompound*(const wire::common::Identifier& id)>;

[[nodiscard]] std::optional<Dialog> read(const wire::nbt::NbtCompound& nbt, const ByName& byName);

} // namespace basalt::server::runtime::world::dialogs
