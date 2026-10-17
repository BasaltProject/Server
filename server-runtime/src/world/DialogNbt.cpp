#include <basalt/server/runtime/world/Dialogs.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

namespace basalt::server::runtime::world::dialogs {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

[[nodiscard]] NbtTag textTag(const wire::text::Component& text) { return wire::text::componentNbt::serialize(text); }

[[nodiscard]] NbtCompound inputNbt(const Input& input) {
    NbtCompound out;
    out.putString("key", input.key);
    out.set("label", textTag(input.label));
    switch (input.kind) {
    case InputKind::Text:
        out.putString("type", "minecraft:text");
        out.putBoolean("label_visible", input.labelVisible);
        out.putString("initial", input.initialText);
        out.putInt("max_length", input.maxLength);
        break;
    case InputKind::Boolean:
        out.putString("type", "minecraft:boolean");
        out.putBoolean("initial", input.initialBool);
        out.putString("on_true", input.onTrue);
        out.putString("on_false", input.onFalse);
        break;
    case InputKind::NumberRange:
        out.putString("type", "minecraft:number_range");
        out.putFloat("start", input.start);
        out.putFloat("end", input.end);
        if (input.initialNumber.has_value()) {
            out.putFloat("initial", *input.initialNumber);
        }
        if (input.step.has_value()) {
            out.putFloat("step", *input.step);
        }
        break;
    case InputKind::SingleOption: {
        out.putString("type", "minecraft:single_option");
        out.putBoolean("label_visible", input.labelVisible);
        std::vector<NbtTag> options;
        for (const Option& option : input.options) {
            NbtCompound entry;
            entry.putString("id", option.id);
            if (option.display.has_value()) {
                entry.set("display", textTag(*option.display));
            }
            entry.putBoolean("initial", option.initial);
            options.push_back(NbtTag::ofCompound(std::move(entry)));
        }
        out.set("options", NbtTag::ofList(NbtList{NbtType::Compound, std::move(options)}));
        break;
    }
    }
    return out;
}

} // namespace

NbtCompound toNbt(const Dialog& dialog, const std::string_view actionId) {
    NbtCompound out;
    out.putString("type", "minecraft:multi_action");
    out.set("title", textTag(dialog.title));
    out.putBoolean("can_close_with_escape", true);
    out.putInt("columns", 1);
    std::vector<NbtTag> body;
    for (const wire::text::Component& line : dialog.body) {
        NbtCompound message;
        message.putString("type", "minecraft:plain_message");
        message.set("contents", textTag(line));
        body.push_back(NbtTag::ofCompound(std::move(message)));
    }
    out.set("body", NbtTag::ofList(NbtList{NbtType::Compound, std::move(body)}));
    std::vector<NbtTag> inputs;
    for (const Input& input : dialog.inputs) {
        inputs.push_back(NbtTag::ofCompound(inputNbt(input)));
    }
    out.set("inputs", NbtTag::ofList(NbtList{NbtType::Compound, std::move(inputs)}));
    std::vector<NbtTag> actions;
    for (std::size_t at = 0; at < dialog.buttons.size(); ++at) {
        NbtCompound additions;
        additions.putInt(kButtonKey, static_cast<std::int32_t>(at));
        NbtCompound action;
        action.putString("type", "minecraft:dynamic/custom");
        action.putString("id", std::string{actionId});
        action.set("additions", NbtTag::ofCompound(std::move(additions)));
        NbtCompound button;
        button.set("label", textTag(dialog.buttons[at].label));
        button.set("action", NbtTag::ofCompound(std::move(action)));
        actions.push_back(NbtTag::ofCompound(std::move(button)));
    }
    out.set("actions", NbtTag::ofList(NbtList{NbtType::Compound, std::move(actions)}));
    return out;
}

} // namespace basalt::server::runtime::world::dialogs
