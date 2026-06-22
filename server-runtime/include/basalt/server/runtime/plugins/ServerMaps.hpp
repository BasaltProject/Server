#pragma once

#include <basalt/server/plugins/PluginMaps.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/maps/MapStorage.hpp>
#include <basalt/server/world/loot/LootContext.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <functional>
#include <vector>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::plugins {

class ServerMaps final : public server::plugins::PluginMaps {
public:
    struct Hooks {
        world::maps::MapStorage* storage = nullptr;
        std::function<std::vector<player::PlayerSession*>()> sessions;
        std::function<world::LoadedWorld*(const std::string& name)> worldFor;
        std::function<bool(world::LoadedWorld&, wire::nbt::NbtCompound&,
                           const server::world::loot::ExplorationMapRequest&, const std::array<double, 3>&)>
            explore;
    };

    explicit ServerMaps(Hooks hooks) : hooks_(std::move(hooks)) {}

    [[nodiscard]] std::optional<server::plugins::PluginMap> map(std::int32_t id) override;
    std::int32_t create(std::string_view world, std::int32_t centerX, std::int32_t centerZ,
                        std::int32_t scale) override;
    bool update(const server::plugins::PluginMap& map) override;
    [[nodiscard]] std::vector<std::uint8_t> colors(std::int32_t id) override;
    [[nodiscard]] std::vector<std::string> carriers(std::int32_t id) override;
    void show(std::string_view uuid, std::int32_t id, std::span<const std::uint8_t> colors,
              const std::vector<server::plugins::PluginMapCursor>& cursors) override;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>>
    explorerMap(std::string_view world, double x, double y, double z,
                const server::plugins::PluginExploration& request) override;

private:
    Hooks hooks_;
};

} // namespace basalt::server::runtime::plugins
