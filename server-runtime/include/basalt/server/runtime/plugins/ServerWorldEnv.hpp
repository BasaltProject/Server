#pragma once

#include <basalt/server/plugins/PluginWorldEnv.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>

#include <functional>
#include <vector>

namespace basalt::server::runtime::plugins {

class ServerWorldEnv final : public server::plugins::PluginWorldEnv {
public:
    ServerWorldEnv(world::LoadedWorld& world, std::shared_ptr<const ModelHooks> hooks,
                   std::function<std::vector<player::PlayerSession*>()> players)
        : world_(&world), hooks_(std::move(hooks)), players_(std::move(players)) {}

    void sound(std::string_view sound, std::string_view category, double x, double y, double z, float volume,
               float pitch, std::int64_t seed) override;
    void particle(const server::plugins::PluginParticle& particle) override;
    void levelEvent(std::int32_t type, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t data,
                    bool global) override;
    [[nodiscard]] server::plugins::PluginBorder border() override;
    void setBorder(const server::plugins::PluginBorder& border, double seconds) override;
    [[nodiscard]] std::int32_t playerCount() override;
    [[nodiscard]] std::int32_t blockEntityCount() override;
    [[nodiscard]] std::optional<std::vector<server::plugins::PluginDrop>>
    blockDrops(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view tool,
               const std::vector<std::pair<std::string, std::int32_t>>& enchantments, bool byEntity) override;
    bool breakBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view tool,
                    const std::vector<std::pair<std::string, std::int32_t>>& enchantments, bool drops) override;
    bool boneMeal(std::int32_t x, std::int32_t y, std::int32_t z) override;
    [[nodiscard]] std::int32_t signalFrom(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t face,
                                          bool direct) override;
    [[nodiscard]] double temperature(std::int32_t x, std::int32_t y, std::int32_t z) override;
    [[nodiscard]] double breakSpeed(std::string_view player, std::int32_t x, std::int32_t y, std::int32_t z) override;
    [[nodiscard]] bool survives(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) override;

private:
    [[nodiscard]] std::vector<player::PlayerSession*> watchers() const;

    world::LoadedWorld* world_;
    std::shared_ptr<const ModelHooks> hooks_;
    std::function<std::vector<player::PlayerSession*>()> players_;
};

} // namespace basalt::server::runtime::plugins
