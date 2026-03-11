#include <basalt/server/runtime/world/Dialogs.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <string_view>

namespace basalt::server::runtime::world::dialogs {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::text::Component;

/// A dialog that leads to a dialog that leads back is a registry an operator can write, so the
/// walk stops rather than following it.
constexpr int kMaxDepth = 4;

[[nodiscard]] Component textOf(const NbtTag* tag) {
    return tag == nullptr ? Component::empty() : wire::text::componentNbt::deserialize(*tag);
}

/// The part of a type name after the namespace, since every one of these is vanilla's.
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

/// The dialog a `show_dialog` points at, which is either written out where it is used or named.
/// A name beginning with a hash is a tag, and this server loads no dialog tags: vanilla's own two
/// lists are tags, and both are empty until a datapack fills them.
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

void readAction(const NbtCompound& action, Button& button, const ByName& byName, const int depth) {
    const std::string_view kind = kindOf(action);
    if (kind == "run_command") {
        button.press = Press::RunCommand;
        button.value = std::string{action.getString("command")};
        return;
    }
    if (kind == "show_dialog") {
        const NbtTag* next = action.find("dialog");
        button.next = next == nullptr || depth >= kMaxDepth ? nullptr : pointedAt(*next, byName, depth);
        button.press = button.next == nullptr ? Press::None : Press::OpenDialog;
        return;
    }
    // Everything else is a line the player can read and act on themselves.
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

/// One entry of a `dialog_list`, which is a whole dialog rather than a button: the label is its
/// own title, and pressing it opens it.
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
    const NbtList* inputs = nbt.getList("inputs");
    dialog.hasInputs = inputs != nullptr && !inputs->empty();

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
        // An unknown kind is still worth drawing: its title and body are where the words are.
        return dialog;
    }
    if (const NbtCompound* exit = nbt.getCompound("exit_action"); exit != nullptr) {
        dialog.buttons.push_back(readButton(*exit, byName, depth));
    }
    return dialog;
}

} // namespace

std::optional<Dialog> read(const NbtCompound& nbt, const ByName& byName) { return readAt(nbt, byName, 0); }

} // namespace basalt::server::runtime::world::dialogs
