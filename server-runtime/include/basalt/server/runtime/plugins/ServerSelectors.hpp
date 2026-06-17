#pragma once

#include <basalt/server/plugins/PluginSelectors.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>

#include <functional>
#include <vector>

namespace basalt::server::runtime::plugins {

class ServerSelectors final : public server::plugins::PluginSelectors {
public:
    struct Hooks {
        std::function<const command::CommandEnvironment&()> environment;
        std::function<const command::CommandSource*()> console;
        std::function<std::vector<player::PlayerSession*>()> sessions;
    };

    explicit ServerSelectors(Hooks hooks) : hooks_(std::move(hooks)) {}

    [[nodiscard]] bool select(std::string_view selector, std::string_view asUuid,
                              std::vector<server::plugins::PluginSelected>& into, std::string& error) override;

private:
    Hooks hooks_;
};

} // namespace basalt::server::runtime::plugins
