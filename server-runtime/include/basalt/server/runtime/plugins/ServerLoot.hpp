#pragma once

#include <basalt/server/plugins/PluginLoot.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>

#include <functional>
#include <vector>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::plugins {

class ServerLoot final : public server::plugins::PluginLoot {
public:
    struct Hooks {
        world::RealLootTables* tables = nullptr;
        std::function<world::LoadedWorld*(const std::string& name)> worldFor;
        std::function<std::vector<player::PlayerSession*>()> sessions;
    };

    explicit ServerLoot(Hooks hooks) : hooks_(std::move(hooks)) {}

    [[nodiscard]] std::vector<std::string> tables() override;
    [[nodiscard]] bool has(std::string_view table) override;
    [[nodiscard]] std::optional<std::vector<server::plugins::PluginLootItem>>
    roll(std::string_view table, const server::plugins::PluginLootRoll& roll) override;

private:
    Hooks hooks_;
};

} // namespace basalt::server::runtime::plugins
