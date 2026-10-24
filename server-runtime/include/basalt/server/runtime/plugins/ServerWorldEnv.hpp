#pragma once

#include <basalt/server/plugins/PluginWorldEnv.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>

#include <functional>
#include <vector>

namespace basalt::server::runtime::plugins {

/// The downfall (climate humidity) of a biome by its path, 0.4 when the biome is unknown.
[[nodiscard]] float biomeDownfall(std::string_view path);

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
    void blockEvent(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t first, std::int32_t second) override;
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
    [[nodiscard]] double humidity(std::int32_t x, std::int32_t y, std::int32_t z) override;
    [[nodiscard]] double breakSpeed(std::string_view player, std::int32_t x, std::int32_t y, std::int32_t z) override;
    [[nodiscard]] std::vector<server::plugins::PluginStructureStart> structureStarts(std::int32_t chunkX,
                                                                                     std::int32_t chunkZ) override;
    [[nodiscard]] std::vector<server::plugins::PluginRaid> raids() override;
    bool setRaid(std::int32_t id, std::int32_t omenLevel, std::int32_t totalGroups) override;
    [[nodiscard]] server::plugins::PluginFluid fluidAt(std::int32_t x, std::int32_t y, std::int32_t z) override;
    [[nodiscard]] std::optional<std::array<std::int32_t, 3>> lightningTarget(std::int32_t x, std::int32_t z) override;
    void gameEvent(std::string_view event, double x, double y, double z, std::string_view source) override;
    bool growTree(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view sapling) override;
    [[nodiscard]] std::vector<server::plugins::PluginPoi> pointsOfInterest(const std::vector<std::string>& types,
                                                                           std::int32_t x, std::int32_t y,
                                                                           std::int32_t z, std::int32_t radius,
                                                                           std::int32_t occupancy) override;
    [[nodiscard]] std::optional<std::vector<server::plugins::PluginPlacedBlock>>
    placeTree(std::string_view tree, std::int32_t x, std::int32_t y, std::int32_t z, bool apply) override;
    bool randomTick(std::int32_t x, std::int32_t y, std::int32_t z) override;
    [[nodiscard]] std::int32_t tickableBlockEntityCount() override;
    [[nodiscard]] std::optional<double> tunable(std::string_view key) override;
    bool setTunable(std::string_view key, double value) override;
    [[nodiscard]] server::plugins::PluginSpawnTunables spawnTunables() override;
    void setSpawnTunables(const server::plugins::PluginSpawnTunables& tunables) override;
    [[nodiscard]] std::int32_t viewDistance() override;
    void setViewDistance(std::int32_t chunks) override;
    [[nodiscard]] std::int32_t seaLevel() override;
    [[nodiscard]] std::vector<std::int32_t> forcedChunks() override;
    void setChunkForced(std::int32_t chunkX, std::int32_t chunkZ, bool forced) override;
    [[nodiscard]] std::array<std::int32_t, 3> weatherTimes() override;
    void setWeatherTimes(std::int32_t rain, std::int32_t thunder, std::int32_t clear) override;
    [[nodiscard]] std::int64_t inhabitedTime(std::int32_t chunkX, std::int32_t chunkZ) override;
    void setInhabitedTime(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t ticks) override;
    bool useChunkFill(ChunkFill fill) override;
    bool useChunkFill(ChunkFill fill, const ChunkFillTraits& traits) override;
    [[nodiscard]] bool survives(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) override;

private:
    [[nodiscard]] std::vector<player::PlayerSession*> watchers() const;

    world::LoadedWorld* world_;
    std::shared_ptr<const ModelHooks> hooks_;
    std::function<std::vector<player::PlayerSession*>()> players_;
};

} // namespace basalt::server::runtime::plugins
