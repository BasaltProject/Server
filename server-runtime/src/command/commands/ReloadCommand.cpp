#include <basalt/server/runtime/command/commands/ReloadCommand.hpp>

#include <cstdint>
#include <string>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::text::Component;

} // namespace

ReloadCommand::ReloadCommand(config::LiveConfigs& live, std::function<void()> onReloaded, DataPackControl packs,
                             config::ConfigDirectory directory)
    : Command("reload", permissionLevel::kGamemasters), live_(&live), onReloaded_(std::move(onReloaded)),
      packs_(std::move(packs)), directory_(std::move(directory)) {}

void ReloadCommand::build(const CommandNodeBuilder& root) const {
    const auto reload = [this](const CommandContext& context) -> Parsed<std::int32_t> {
        // Every file first, then the swap: a reload that applied three files and gave up on the
        // fourth would leave the server in a state no file on disk describes.
        std::expected<config::ConfigSnapshot, std::string> next =
            config::LiveConfigs::read(directory_, context.environment().logger());
        if (next.has_value()) {
            live_->replace(std::move(*next));
            if (onReloaded_) {
                onReloaded_();
            }
            context.environment().logger().info(
                [] { return std::string{"Reloaded the configuration; server.toml and worlds.toml need a restart."}; });
        }
        // parity: `discoverNewPacks`, then "Reloading!" before the load itself.
        const std::vector<std::string> selected =
            packs_.packs == nullptr ? std::vector<std::string>{} : packs_.packs->discoverNewPacks();
        context.success(Component::translatable("commands.reload.success"), true);
        if (packs_.reload && !packs_.reload(selected)) {
            context.dispatcher().feedback().failure(context.source(),
                                                    Component::translatable("commands.reload.failure"));
        }
        if (!next.has_value()) {
            context.dispatcher().feedback().failure(context.source(),
                                                    Component::text("Configuration not reloaded: " + next.error()));
        }
        return 0;
    };
    root.executes(reload);
}

} // namespace basalt::server::runtime::command::commands
