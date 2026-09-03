#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/command/commands/ImportPlayersCommand.hpp>
#include <basalt/server/runtime/world/VanillaSaveImport.hpp>

#include <string>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::text::Component;

/// A path as an operator typed it: trailing separators and stray spaces are theirs, not the save's.
[[nodiscard]] std::string tidied(std::string path) {
    while (!path.empty() && (path.back() == ' ' || path.back() == '/')) {
        path.pop_back();
    }
    const std::size_t first = path.find_first_not_of(' ');
    return first == std::string::npos ? std::string{} : path.substr(first);
}

} // namespace

ImportPlayersCommand::ImportPlayersCommand(std::function<std::int64_t()> nowEpochSeconds)
    : Command("import-players", permissionLevel::kOwners), nowEpochSeconds_(std::move(nowEpochSeconds)) {}

void ImportPlayersCommand::build(const CommandNodeBuilder& root) const {
    root.argument("path", greedyStringArgument(), [this](const CommandNodeBuilder& path) {
        path.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            storage::PlayerRepository* repository = context.environment().options().playerRepository;
            if (repository == nullptr) {
                return std::unexpected{
                    CommandSyntaxError{.component = Component::text("This server has no player database.")}};
            }
            const auto* asked = context.get<std::string>("path");
            const std::string savePath = tidied(asked == nullptr ? std::string{} : *asked);
            const std::expected<world::ImportReport, std::string> report =
                world::importVanillaSave(savePath, *repository, context.environment().options().defaultWorldName,
                                         nowEpochSeconds_(), context.environment().logger());
            if (!report.has_value()) {
                return std::unexpected{CommandSyntaxError{.component = Component::text(report.error())}};
            }
            context.success(Component::text("Imported " + std::to_string(report->imported) + " player(s) from " +
                                            savePath + " (" + std::to_string(report->skipped) + " had no data, " +
                                            std::to_string(report->failed) + " unreadable)."),
                            true);
            return report->imported;
        });
    });
}

} // namespace basalt::server::runtime::command::commands
