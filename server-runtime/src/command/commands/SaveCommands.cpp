#include <basalt/server/runtime/command/commands/SaveCommands.hpp>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::text::Component;

} // namespace

bool SaveControl::setEnabled(bool value) {
    if (enabled_ == value) {
        return false;
    }
    enabled_ = value;
    return true;
}

void SaveControl::save() const {
    if (saveNow_) {
        saveNow_();
    }
}

SaveAllCommand::SaveAllCommand(SaveControl& control)
    : Command("save-all", permissionLevel::kAdmins), control_(&control) {}

void SaveAllCommand::build(const CommandNodeBuilder& root) const {
    const auto saveAll = [this](const CommandContext& context) -> Parsed<std::int32_t> {
        context.success(Component::translatable("commands.save.saving"));
        control_->save();
        context.success(Component::translatable("commands.save.success"), true);
        return 1;
    };
    root.executes(saveAll);
    root.literal("flush", [&saveAll](const CommandNodeBuilder& flush) { flush.executes(saveAll); });
}

SaveOffCommand::SaveOffCommand(SaveControl& control)
    : Command("save-off", permissionLevel::kAdmins), control_(&control) {}

void SaveOffCommand::build(const CommandNodeBuilder& root) const {
    root.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
        if (!control_->setEnabled(false)) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.save.alreadyOff")}};
        }
        context.success(Component::translatable("commands.save.disabled"), true);
        return 1;
    });
}

SaveOnCommand::SaveOnCommand(SaveControl& control) : Command("save-on", permissionLevel::kAdmins), control_(&control) {}

void SaveOnCommand::build(const CommandNodeBuilder& root) const {
    root.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
        if (!control_->setEnabled(true)) {
            return std::unexpected{CommandSyntaxError{.component = Component::translatable("commands.save.alreadyOn")}};
        }
        context.success(Component::translatable("commands.save.enabled"), true);
        return 1;
    });
}

} // namespace basalt::server::runtime::command::commands
