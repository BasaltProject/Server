#include <basalt/server/runtime/command/commands/ReloadCommand.hpp>

#include <string>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::text::Component;
using wire::text::kGray;

} // namespace

ReloadCommand::ReloadCommand(config::LiveConfigs& live, std::function<void()> onReloaded,
                             std::function<std::int32_t()> reloadFunctions, config::ConfigDirectory directory)
    : Command("reload", permissionLevel::kAdmins), live_(&live), onReloaded_(std::move(onReloaded)),
      reloadFunctions_(std::move(reloadFunctions)), directory_(std::move(directory)) {}

void ReloadCommand::build(const CommandNodeBuilder& root) const {
    const auto reload = [this](const CommandContext& context) -> Parsed<std::int32_t> {
        // Every file first, then the swap: a reload that applied three files and gave up on the
        // fourth would leave the server in a state no file on disk describes.
        std::expected<config::ConfigSnapshot, std::string> next =
            config::LiveConfigs::read(directory_, context.environment().logger());
        if (!next.has_value()) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::text("Nothing was reloaded: " + next.error())}};
        }
        live_->replace(std::move(*next));
        if (onReloaded_) {
            onReloaded_();
        }
        const std::int32_t functionCount = reloadFunctions_ ? reloadFunctions_() : 0;

        const std::size_t files = config::LiveConfigs::kReloadableFiles.size();
        context.success(Component::text("Reloaded " + std::to_string(files) + " configuration file(s) and " +
                                        std::to_string(functionCount) + " function(s)."),
                        true);
        std::string named;
        for (const std::string_view file : config::LiveConfigs::kReloadableFiles) {
            named += named.empty() ? std::string{file} : ", " + std::string{file};
        }
        context.source().sendSystemMessage(Component::text("  " + named, kGray));
        // Said every time rather than only when one of them changed: an operator who edited
        // server.toml and saw "reloaded" would reasonably believe their edit had taken.
        context.source().sendSystemMessage(
            Component::text("  server.toml and worlds.toml are NOT reloaded: the port is already bound, the worlds "
                            "are already loaded, and applying half of either is worse than not applying it. "
                            "Restart for those.",
                            kGray));
        return static_cast<std::int32_t>(files);
    };
    root.executes(reload);
    root.literal("confirm", [reload](const CommandNodeBuilder& confirm) { confirm.executes(reload); });
}

} // namespace basalt::server::runtime::command::commands
