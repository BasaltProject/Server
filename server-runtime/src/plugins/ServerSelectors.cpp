#include <basalt/server/runtime/command/EntitySelector.hpp>
#include <basalt/server/runtime/plugins/ServerSelectors.hpp>

#include <optional>

namespace basalt::server::runtime::plugins {

bool ServerSelectors::select(std::string_view selector, std::string_view asUuid,
                             std::vector<server::plugins::PluginSelected>& into, std::string& error) {
    if (!hooks_.environment) {
        error = "selectors are not available";
        return false;
    }
    const command::CommandEnvironment& environment = hooks_.environment();
    std::optional<command::PlayerCommandSource> player;
    if (!asUuid.empty()) {
        for (player::PlayerSession* session :
             hooks_.sessions ? hooks_.sessions() : std::vector<player::PlayerSession*>{}) {
            if (session->uuid().toString() == asUuid) {
                player.emplace(*session, environment.groups());
            }
        }
    }
    const command::CommandSource* source = player.has_value() ? &*player : hooks_.console ? hooks_.console() : nullptr;
    if (source == nullptr) {
        error = "there is nobody to select for";
        return false;
    }
    command::StringReader reader{std::string{selector}};
    const command::Parsed<command::EntitySelector> parsed =
        command::EntitySelector::parse(reader, *source, environment);
    if (!parsed.has_value()) {
        error = "that is not a valid selector";
        return false;
    }
    for (const command::CommandTarget& target : parsed->select(*source, environment, false)) {
        into.push_back(server::plugins::PluginSelected{
            .uuid = target.uuid(), .world = target.worldName(), .player = target.player() != nullptr});
    }
    return true;
}

} // namespace basalt::server::runtime::plugins
