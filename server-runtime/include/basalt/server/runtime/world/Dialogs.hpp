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

/// parity: `ClickEvent.Action`.
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
    std::string value;
    /// Shared because a button may point at a registry entry that several buttons also point at.
    std::shared_ptr<const Dialog> next;
};

struct Dialog {
    wire::text::Component title;
    std::vector<wire::text::Component> body;
    std::vector<Button> buttons;
    /// parity: `CommonDialogData.inputs`.
    bool hasInputs = false;
};

using ByName = std::function<const wire::nbt::NbtCompound*(const wire::common::Identifier& id)>;

[[nodiscard]] std::optional<Dialog> read(const wire::nbt::NbtCompound& nbt, const ByName& byName);

} // namespace basalt::server::runtime::world::dialogs
