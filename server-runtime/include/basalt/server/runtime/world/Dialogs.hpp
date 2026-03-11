#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::world::dialogs {

/// What pressing a button does. parity: `ClickEvent.Action`, of which only two do anything on a
/// client with no dialogs of its own; a link, a line to copy or a command to suggest is text that
/// client can read and nothing more.
enum class Press : std::uint8_t {
    None,
    RunCommand,
    ShowText,
    OpenDialog,
};

struct Dialog;

struct Button {
    wire::text::Component label;
    Press press = Press::None;
    /// The command to run, or the text to show.
    std::string value;
    /// The dialog this button leads to. Shared rather than held: a button may point at a registry
    /// entry, and several of them may point at the same one.
    std::shared_ptr<const Dialog> next;
};

/// One dialog, flattened to what a screen made of buttons can draw.
struct Dialog {
    wire::text::Component title;
    /// Every plain message in the body, in order. An item body has nothing a form can draw.
    std::vector<wire::text::Component> body;
    std::vector<Button> buttons;
    /// parity: `CommonDialogData.inputs`. A form of buttons cannot ask for any of them, so a
    /// dialog that has them is drawn without and says so.
    bool hasInputs = false;
};

/// Resolves a dialog named rather than written out where it is used.
using ByName = std::function<const wire::nbt::NbtCompound*(const wire::common::Identifier& id)>;

/// Reads one dialog from the nbt the registry holds. Nothing when the tag is not a dialog.
[[nodiscard]] std::optional<Dialog> read(const wire::nbt::NbtCompound& nbt, const ByName& byName);

} // namespace basalt::server::runtime::world::dialogs
