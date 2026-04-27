#include <basalt/server/runtime/world/Dialogs.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <array>
#include <charconv>
#include <cmath>
#include <string_view>

namespace basalt::server::runtime::world::dialogs {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::text::Component;

constexpr int kMaxDepth = 4;

[[nodiscard]] Component textOf(const NbtTag* tag) {
    return tag == nullptr ? Component::empty() : wire::text::componentNbt::deserialize(*tag);
}

[[nodiscard]] std::string_view kindOf(const NbtCompound& nbt) {
    const std::string_view named = nbt.getString("type");
    const std::size_t colon = named.find(':');
    return colon == std::string_view::npos ? named : named.substr(colon + 1);
}

void readBodyInto(const NbtTag& tag, std::vector<Component>& body) {
    const auto plain = [&body](const NbtCompound& one) {
        // parity: the other kind is an item, which a screen of buttons has nowhere to draw.
        if (kindOf(one) == "plain_message") {
            body.push_back(textOf(one.find("contents")));
        }
    };
    if (const NbtCompound* one = tag.get<NbtCompound>(); one != nullptr) {
        plain(*one);
        return;
    }
    const NbtList* many = tag.get<NbtList>();
    if (many == nullptr) {
        return;
    }
    for (const NbtTag& entry : *many) {
        if (const NbtCompound* one = entry.get<NbtCompound>(); one != nullptr) {
            plain(*one);
        }
    }
}

[[nodiscard]] std::optional<Dialog> readAt(const NbtCompound& nbt, const ByName& byName, int depth);

[[nodiscard]] std::shared_ptr<const Dialog> pointedAt(const NbtTag& tag, const ByName& byName, const int depth) {
    const NbtCompound* inline_ = tag.get<NbtCompound>();
    if (inline_ == nullptr) {
        const auto* named = tag.get<std::string>();
        if (named == nullptr || !byName) {
            return nullptr;
        }
        inline_ = byName(Identifier::parse(*named).value_or(Identifier::vanilla("")));
        if (inline_ == nullptr) {
            return nullptr;
        }
    }
    std::optional<Dialog> read = readAt(*inline_, byName, depth + 1);
    return read.has_value() ? std::make_shared<const Dialog>(std::move(*read)) : nullptr;
}

[[nodiscard]] float numberOf(const NbtTag* tag, const float fallback) {
    if (tag == nullptr) {
        return fallback;
    }
    if (const auto* v = tag->get<float>(); v != nullptr) {
        return *v;
    }
    if (const auto* v = tag->get<double>(); v != nullptr) {
        return static_cast<float>(*v);
    }
    if (const auto* v = tag->get<std::int32_t>(); v != nullptr) {
        return static_cast<float>(*v);
    }
    if (const auto* v = tag->get<std::int16_t>(); v != nullptr) {
        return static_cast<float>(*v);
    }
    if (const auto* v = tag->get<std::int8_t>(); v != nullptr) {
        return static_cast<float>(*v);
    }
    return fallback;
}

[[nodiscard]] bool flagOf(const NbtCompound& nbt, std::string_view key, const bool fallback) {
    const NbtTag* tag = nbt.find(key);
    if (tag == nullptr) {
        return fallback;
    }
    if (const auto* v = tag->get<std::int8_t>(); v != nullptr) {
        return *v != 0;
    }
    return fallback;
}

void readInputsInto(const NbtCompound& nbt, std::vector<Input>& inputs) {
    const NbtList* listed = nbt.getList("inputs");
    if (listed == nullptr) {
        return;
    }
    for (const NbtTag& entry : *listed) {
        const NbtCompound* one = entry.get<NbtCompound>();
        if (one == nullptr) {
            continue;
        }
        Input input;
        input.key = std::string{one->getString("key")};
        input.label = textOf(one->find("label"));
        const std::string_view kind = kindOf(*one);
        if (kind == "text") {
            input.kind = InputKind::Text;
            input.labelVisible = flagOf(*one, "label_visible", true);
            input.initialText = std::string{one->getString("initial")};
            input.maxLength =
                one->contains("max_length") ? static_cast<std::int32_t>(numberOf(one->find("max_length"), 32.0F)) : 32;
            input.multiline = one->getCompound("multiline") != nullptr;
        } else if (kind == "boolean") {
            input.kind = InputKind::Boolean;
            input.initialBool = flagOf(*one, "initial", false);
            input.onTrue = one->contains("on_true") ? std::string{one->getString("on_true")} : "true";
            input.onFalse = one->contains("on_false") ? std::string{one->getString("on_false")} : "false";
        } else if (kind == "number_range") {
            input.kind = InputKind::NumberRange;
            input.start = numberOf(one->find("start"), 0.0F);
            input.end = numberOf(one->find("end"), 0.0F);
            if (one->contains("initial")) {
                input.initialNumber = numberOf(one->find("initial"), 0.0F);
            }
            if (one->contains("step")) {
                input.step = numberOf(one->find("step"), 1.0F);
            }
        } else if (kind == "single_option") {
            input.kind = InputKind::SingleOption;
            input.labelVisible = flagOf(*one, "label_visible", true);
            if (const NbtList* options = one->getList("options"); options != nullptr) {
                for (const NbtTag& option : *options) {
                    Option read;
                    if (const auto* plain = option.get<std::string>(); plain != nullptr) {
                        read.id = *plain;
                    } else if (const NbtCompound* full = option.get<NbtCompound>(); full != nullptr) {
                        read.id = std::string{full->getString("id")};
                        read.initial = flagOf(*full, "initial", false);
                        if (const NbtTag* display = full->find("display"); display != nullptr) {
                            read.display = textOf(display);
                        }
                    }
                    input.options.push_back(std::move(read));
                }
            }
        } else {
            continue;
        }
        inputs.push_back(std::move(input));
    }
}

void readAction(const NbtCompound& action, Button& button, const ByName& byName, const int depth) {
    const std::string_view kind = kindOf(action);
    if (kind == "run_command") {
        button.press = Press::RunCommand;
        button.value = std::string{action.getString("command")};
        return;
    }
    if (kind == "dynamic/run_command") {
        button.press = Press::RunTemplate;
        button.value = std::string{action.getString("template")};
        return;
    }
    if (kind == "show_dialog") {
        const NbtTag* next = action.find("dialog");
        button.next = next == nullptr || depth >= kMaxDepth ? nullptr : pointedAt(*next, byName, depth);
        button.press = button.next == nullptr ? Press::None : Press::OpenDialog;
        return;
    }
    for (const std::string_view field : {"url", "value", "command"}) {
        if (action.contains(field)) {
            button.press = Press::ShowText;
            button.value = std::string{action.getString(field)};
            return;
        }
    }
}

[[nodiscard]] Button readButton(const NbtCompound& nbt, const ByName& byName, const int depth) {
    Button button;
    button.label = textOf(nbt.find("label"));
    if (const NbtCompound* action = nbt.getCompound("action"); action != nullptr) {
        readAction(*action, button, byName, depth);
    }
    return button;
}

void readButtonsInto(const NbtCompound& nbt, std::string_view key, std::vector<Button>& buttons, const ByName& byName,
                     const int depth) {
    const NbtList* listed = nbt.getList(key);
    if (listed == nullptr) {
        return;
    }
    for (const NbtTag& entry : *listed) {
        if (const NbtCompound* one = entry.get<NbtCompound>(); one != nullptr) {
            buttons.push_back(readButton(*one, byName, depth));
        }
    }
}

void readListedInto(const NbtCompound& nbt, std::vector<Button>& buttons, const ByName& byName, const int depth) {
    const NbtTag* listed = nbt.find("dialogs");
    if (listed == nullptr || depth >= kMaxDepth) {
        return;
    }
    const auto one = [&buttons, &byName, depth](const NbtTag& tag) {
        std::shared_ptr<const Dialog> next = pointedAt(tag, byName, depth);
        if (next == nullptr) {
            return;
        }
        Button button;
        button.label = next->title;
        button.press = Press::OpenDialog;
        button.next = std::move(next);
        buttons.push_back(std::move(button));
    };
    if (const NbtList* many = listed->get<NbtList>(); many != nullptr) {
        for (const NbtTag& entry : *many) {
            one(entry);
        }
        return;
    }
    one(*listed);
}

std::optional<Dialog> readAt(const NbtCompound& nbt, const ByName& byName, const int depth) {
    const std::string_view kind = kindOf(nbt);
    if (kind.empty()) {
        return std::nullopt;
    }
    Dialog dialog;
    dialog.title = textOf(nbt.find("title"));
    if (const NbtTag* body = nbt.find("body"); body != nullptr) {
        readBodyInto(*body, dialog.body);
    }
    readInputsInto(nbt, dialog.inputs);

    if (kind == "notice") {
        // parity: a notice with no action of its own still has a button, which only closes it.
        if (const NbtCompound* action = nbt.getCompound("action"); action != nullptr) {
            dialog.buttons.push_back(readButton(*action, byName, depth));
        }
    } else if (kind == "confirmation") {
        for (const std::string_view side : {"yes", "no"}) {
            if (const NbtCompound* action = nbt.getCompound(side); action != nullptr) {
                dialog.buttons.push_back(readButton(*action, byName, depth));
            }
        }
    } else if (kind == "multi_action") {
        readButtonsInto(nbt, "actions", dialog.buttons, byName, depth);
    } else if (kind == "dialog_list") {
        readListedInto(nbt, dialog.buttons, byName, depth);
    } else if (kind != "server_links") {
        return dialog;
    }
    if (const NbtCompound* exit = nbt.getCompound("exit_action"); exit != nullptr) {
        dialog.buttons.push_back(readButton(*exit, byName, depth));
    }
    return dialog;
}

} // namespace

std::optional<Dialog> read(const NbtCompound& nbt, const ByName& byName) { return readAt(nbt, byName, 0); }

std::string escapeText(const std::string_view text) {
    std::string out;
    for (const char c : text) {
        switch (c) {
        case '"':
        case '\'':
        case '\\':
            out += '\\';
            out += c;
            break;
        case '\b':
            out += "\\b";
            break;
        case '\t':
            out += "\\t";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\r':
            out += "\\r";
            break;
        default:
            out += c;
        }
    }
    return out;
}

std::string numberText(const float value) {
    const auto whole = static_cast<std::int32_t>(value);
    if (static_cast<float>(whole) == value) {
        return std::to_string(whole);
    }
    std::array<char, 32> buffer{};
    const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return error == std::errc{} ? std::string{buffer.data(), end} : std::to_string(value);
}

float initialSlider(const Input& input) {
    const float value = input.initialNumber.value_or((input.start + input.end) / 2.0F);
    return input.start == input.end ? 0.5F : (value - input.start) / (input.end - input.start);
}

float scaledNumber(const Input& input, const float sliderValue) {
    const float inRange = input.start + (input.end - input.start) * sliderValue;
    if (!input.step.has_value()) {
        return inRange;
    }
    const float initial = input.initialNumber.value_or((input.start + input.end) / 2.0F);
    const float step = *input.step;
    const auto stepsOut = static_cast<std::int32_t>(std::floor((inRange - initial) / step + 0.5F));
    const float result = initial + static_cast<float>(stepsOut) * step;
    const float position = input.start == input.end ? 0.5F : (result - input.start) / (input.end - input.start);
    if (position >= 0.0F && position <= 1.0F) {
        return result;
    }
    const std::int32_t sign = stepsOut > 0 ? 1 : (stepsOut < 0 ? -1 : 0);
    return initial + static_cast<float>(stepsOut - sign) * step;
}

std::string instantiate(const std::string_view text, const std::map<std::string, std::string>& values) {
    std::string out;
    std::size_t at = 0;
    while (at < text.size()) {
        const std::size_t dollar = text.find("$(", at);
        const std::size_t close = dollar == std::string_view::npos ? dollar : text.find(')', dollar);
        if (close == std::string_view::npos) {
            out += text.substr(at);
            break;
        }
        out += text.substr(at, dollar - at);
        const auto found = values.find(std::string{text.substr(dollar + 2, close - dollar - 2)});
        if (found != values.end()) {
            out += found->second;
        }
        at = close + 1;
    }
    return out;
}

} // namespace basalt::server::runtime::world::dialogs
