#pragma once

#include "WorldBlockUpdates.hpp"

#include <basalt/server/runtime/plugins/EntityOps.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/plugins/ServerWorldEnv.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::plugins::worldModel {
using server::plugins::PluginEntityRef;
using server::plugins::PluginHologramRef;
using server::plugins::PluginWorldRef;

inline constexpr std::string_view kBukkitValues = "PublicBukkitValues";

const wire::common::Identifier& textDisplay();

class WorldEntityRef : public PluginEntityRef {
public:
    WorldEntityRef(world::LoadedWorld& world, std::int32_t entityId, std::string type,
                   std::shared_ptr<const ModelHooks> hooks = nullptr,
                   std::function<std::vector<player::PlayerSession*>()> players = nullptr);
    [[nodiscard]] std::int32_t id() const override;
    [[nodiscard]] std::string type() const override;
    [[nodiscard]] std::string world() const override;
    [[nodiscard]] double x() const override;
    [[nodiscard]] double y() const override;
    [[nodiscard]] double z() const override;
    [[nodiscard]] bool alive() const override;
    [[nodiscard]] double health() const override;
    void setHealth(double value) override;
    void remove() override;
    bool teleport(double x, double y, double z) override;
    [[nodiscard]] std::string uuid() const override;
    [[nodiscard]] std::array<float, 2> rotation() const override;
    bool setRotation(float yaw, float pitch) override;
    [[nodiscard]] std::array<double, 3> velocity() const override;
    bool setVelocity(double x, double y, double z) override;
    [[nodiscard]] bool onGround() const override;
    [[nodiscard]] std::int32_t age() const override;
    [[nodiscard]] double width() const override;
    [[nodiscard]] double height() const override;
    [[nodiscard]] std::optional<std::string> customName() const override;
    [[nodiscard]] std::optional<server::plugins::PluginEntityValue> property(std::string_view name) const override;
    bool setProperty(std::string_view name, const server::plugins::PluginEntityValue& value) override;
    std::optional<server::plugins::PluginEntityValue>
    invoke(std::string_view operation, const std::vector<server::plugins::PluginEntityValue>& arguments) override;

protected:
    [[nodiscard]] entity::ServerEntity* entity() const;
    world::LoadedWorld* world_;

private:
    [[nodiscard]] entityOps::Environment environment() const;
    std::shared_ptr<const ModelHooks> hooks_;
    std::function<std::vector<player::PlayerSession*>()> players_;
    std::int32_t entityId_;
    std::string type_;
};

class HologramRef final : public PluginHologramRef {
public:
    HologramRef(world::LoadedWorld& world, std::int32_t entityId, std::string type);
    [[nodiscard]] std::int32_t id() const override;
    [[nodiscard]] std::string type() const override;
    [[nodiscard]] std::string world() const override;
    [[nodiscard]] double x() const override;
    [[nodiscard]] double y() const override;
    [[nodiscard]] double z() const override;
    [[nodiscard]] bool alive() const override;
    [[nodiscard]] double health() const override;
    void setHealth(double value) override;
    void remove() override;
    bool teleport(double x, double y, double z) override;
    [[nodiscard]] std::string text() const override;
    void setText(std::string_view value) override;
    void showOnlyTo(const std::vector<std::string>& uuids) override;
    void showToAll() override;
    [[nodiscard]] std::optional<std::vector<std::string>> audience() const override;

private:
    class Access : public WorldEntityRef {
    public:
        using WorldEntityRef::entity;
        using WorldEntityRef::WorldEntityRef;
    };

    [[nodiscard]] entity::ServerEntity* held() const;
    mutable Access inner_;
};

class LoadedWorldRef final : public PluginWorldRef {
public:
    LoadedWorldRef(world::LoadedWorld& world, std::shared_ptr<const ModelHooks> hooks,
                   std::function<std::vector<player::PlayerSession*>()> players);
    [[nodiscard]] server::plugins::PluginWorldEnv* env() override;
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::int64_t time() const override;
    [[nodiscard]] bool raining() const override;
    [[nodiscard]] bool thundering() const override;
    [[nodiscard]] std::string difficulty() const override;
    [[nodiscard]] std::optional<std::string> gameRule(std::string_view name) const override;
    bool setGameRule(std::string_view name, std::string_view value) override;
    [[nodiscard]] std::vector<std::string> gameRules() const override;
    [[nodiscard]] std::optional<std::string> block(std::int32_t x, std::int32_t y, std::int32_t z) const override;
    [[nodiscard]] std::optional<std::string> blockState(std::int32_t x, std::int32_t y, std::int32_t z) const override;
    [[nodiscard]] static std::optional<std::int32_t> parseState(std::string_view text);
    bool setBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view block) override;
    [[nodiscard]] std::string dimension() const override;
    [[nodiscard]] std::int64_t seed() const override;
    [[nodiscard]] std::int32_t minHeight() const override;
    [[nodiscard]] std::int32_t height() const override;
    [[nodiscard]] std::array<std::int32_t, 3> spawn() const override;
    void setSpawn(std::int32_t x, std::int32_t y, std::int32_t z) override;
    [[nodiscard]] std::int64_t dayTime() const override;
    void setDayTime(std::int64_t ticks) override;
    void setWeather(bool raining, bool thundering) override;
    bool setDifficulty(std::string_view name) override;
    [[nodiscard]] std::int32_t stateAt(std::int32_t x, std::int32_t y, std::int32_t z, bool load) override;
    bool setStateAt(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, bool notify) override;
    std::int32_t readStates(std::span<const std::int32_t> positions, std::span<std::int32_t> out, bool load) override;
    std::int32_t writeStates(std::span<const std::int32_t> positions, std::span<const std::int32_t> states,
                             bool notify) override;
    bool readSection(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ, bool load,
                     std::span<std::int32_t> out) override;
    bool readSections(std::int32_t chunkX, std::int32_t chunkZ, bool load, std::span<std::int32_t> out) override;
    [[nodiscard]] std::int32_t chunkStatus(std::int32_t chunkX, std::int32_t chunkZ) const override;
    void requestChunk(std::int32_t chunkX, std::int32_t chunkZ, bool generate, std::int32_t priority,
                      std::function<void(bool)> done) override;
    bool holdChunks(std::string_view token, std::span<const std::int32_t> chunks) override;
    void releaseChunks(std::string_view token) override;
    std::int32_t settleBlocks(std::span<const std::int32_t> positions, std::span<const std::int32_t> previous,
                              std::uint32_t effects) override;
    std::int32_t neighborChanged(std::span<const std::int32_t> positions,
                                 std::span<const std::int32_t> sources) override;
    [[nodiscard]] std::int32_t validState(std::int32_t x, std::int32_t y, std::int32_t z,
                                          std::int32_t stateId) override;
    std::int32_t updateShapes(std::span<const std::int32_t> positions, std::span<const std::int32_t> states,
                              bool indirect) override;
    std::int32_t useItemOn(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t face, std::string_view item,
                           float yaw, float pitch) override;
    bool scheduleTick(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t delay, bool fluid) override;
    std::int32_t applyChanges(std::span<const std::int32_t> positions, std::span<const std::int32_t> states,
                              std::uint32_t effects) override;
    bool writeSection(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ,
                      std::span<const std::int32_t> states, bool notify) override;
    std::int32_t applySections(std::int32_t chunkX, std::int32_t chunkZ, std::span<const std::int32_t> sectionYs,
                               std::span<const std::int32_t> states, bool resend,
                               std::span<std::int32_t> previous) override;
    std::int32_t applyChunks(std::span<const std::int32_t> chunks, std::span<const std::int32_t> sectionYs,
                             std::span<const std::int32_t> states, bool resend,
                             std::span<std::int32_t> previous) override;
    [[nodiscard]] std::uint64_t blockVersion() const override;
    void tellBlockEntity(std::int32_t x, std::int32_t y, std::int32_t z, const server::world::ChunkBlockEntity& made);
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> blockEntityNbt(std::int32_t x, std::int32_t y,
                                                                          std::int32_t z) override;
    [[nodiscard]] std::vector<std::int32_t> blockEntityPositions(std::int32_t chunkX, std::int32_t chunkZ) override;
    bool setBlockEntityNbt(std::int32_t x, std::int32_t y, std::int32_t z, std::span<const std::uint8_t> nbt) override;
    bool createBlockEntity(std::int32_t x, std::int32_t y, std::int32_t z, std::span<const std::uint8_t> nbt) override;
    bool removeBlockEntity(std::int32_t x, std::int32_t y, std::int32_t z) override;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> entityNbt(std::int32_t entityId) override;
    std::int32_t spawnEntityNbt(std::span<const std::uint8_t> nbt, std::string_view cause) override;
    [[nodiscard]] std::optional<DragonFightFacts> dragonFight() override;
    bool setDragonPreviouslyKilled(bool value) override;
    bool placeEndGateway(std::int32_t x, std::int32_t y, std::int32_t z) override;
    bool dragonFightAction(std::string_view action, std::int32_t value) override;
    [[nodiscard]] std::optional<std::array<std::int32_t, 2>> locateStructure(std::string_view structure, std::int32_t x,
                                                                             std::int32_t z, std::int32_t cells,
                                                                             bool skipExisting) override;
    [[nodiscard]] std::optional<std::array<std::int32_t, 3>> locateBiome(const std::vector<std::string>& biomes,
                                                                         std::int32_t x, std::int32_t y, std::int32_t z,
                                                                         std::int32_t radius,
                                                                         std::int32_t step) override;
    [[nodiscard]] static std::vector<std::uint8_t> packedData(const wire::nbt::NbtCompound& data);
    [[nodiscard]] static std::optional<wire::nbt::NbtCompound> unpackedData(std::span<const std::uint8_t> nbt);
    [[nodiscard]] std::vector<std::uint8_t> pluginData() override;
    bool setPluginData(std::span<const std::uint8_t> nbt) override;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> chunkPluginData(std::int32_t chunkX,
                                                                           std::int32_t chunkZ) override;
    bool setChunkPluginData(std::int32_t chunkX, std::int32_t chunkZ, std::span<const std::uint8_t> nbt) override;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> blockEntityPluginData(std::int32_t x, std::int32_t y,
                                                                                 std::int32_t z) override;
    bool setBlockEntityPluginData(std::int32_t x, std::int32_t y, std::int32_t z,
                                  std::span<const std::uint8_t> nbt) override;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> entityPluginData(std::int32_t entityId) override;
    bool setEntityPluginData(std::int32_t entityId, std::span<const std::uint8_t> nbt) override;
    bool readBiomes(std::int32_t chunkX, std::int32_t chunkZ, std::span<std::int32_t> out) override;
    bool writeBiomes(std::int32_t chunkX, std::int32_t chunkZ, std::span<const std::int32_t> ids) override;
    void lightJob(std::span<const std::int32_t> chunks, std::function<void(std::int32_t)> done) override;
    [[nodiscard]] std::int32_t lightAt(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t layer) override;
    bool readLight(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ, std::int32_t layer, bool load,
                   std::span<std::uint8_t> out) override;
    bool writeLight(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ, std::int32_t layer,
                    std::span<const std::uint8_t> nibbles) override;
    void checkLight(std::span<const std::int32_t> positions) override;
    void resendChunks(std::span<const std::int32_t> chunks) override;
    [[nodiscard]] std::optional<std::string> biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const override;
    bool setBiomeAt(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view biome) override;
    [[nodiscard]] std::int32_t highest(std::int32_t x, std::int32_t z, std::string_view kind, bool load) override;
    [[nodiscard]] bool chunkLoaded(std::int32_t chunkX, std::int32_t chunkZ) const override;
    bool loadChunk(std::int32_t chunkX, std::int32_t chunkZ, bool generate) override;
    bool unloadChunk(std::int32_t chunkX, std::int32_t chunkZ) override;
    [[nodiscard]] std::vector<std::int32_t> loadedChunks() const override;
    [[nodiscard]] std::shared_ptr<PluginEntityRef> entityById(std::int32_t id) const override;
    [[nodiscard]] std::shared_ptr<PluginEntityRef> entityByUuid(std::string_view uuid) const override;
    [[nodiscard]] std::vector<std::shared_ptr<PluginEntityRef>> entitiesIn(std::int32_t chunkX,
                                                                           std::int32_t chunkZ) const override;
    void save() override;
    [[nodiscard]] std::string folder() const override;
    [[nodiscard]] std::vector<std::shared_ptr<PluginEntityRef>> entities() const override;
    std::shared_ptr<PluginEntityRef> spawn(std::string_view type, double x, double y, double z) override;
    std::int32_t dropItem(std::span<const std::uint8_t> stack, double x, double y, double z, double velocityX,
                          double velocityY, double velocityZ) override;
    bool explode(double x, double y, double z, float power, bool breakBlocks, std::int32_t sourceEntityId) override;
    std::shared_ptr<PluginEntityRef> spawnAs(std::string_view type, double x, double y, double z, float yaw,
                                             std::string_view reason) override;
    std::shared_ptr<PluginHologramRef> hologram(std::string_view text, double x, double y, double z) override;
    std::shared_ptr<server::plugins::PluginNpcRef> npc(std::string_view name, double x, double y, double z,
                                                       std::function<void(const std::string&)> onClick) override;

private:
    [[nodiscard]] server::world::ChunkHeight chunkHeight() const;
    [[nodiscard]] static std::int32_t biomeIndexOf(std::int32_t biomeId);
    [[nodiscard]] static std::optional<std::int32_t> biomeIdAt(std::int32_t index);
    static void awaitChunk(world::LoadedWorld* world, std::int32_t chunkX, std::int32_t chunkZ, std::int32_t waited,
                           std::shared_ptr<std::function<void(bool)>> done);
    void ensureChunk(std::int32_t chunkX, std::int32_t chunkZ);
    static constexpr std::size_t kLightBytes = 2048;
    [[nodiscard]] bool hasSkylight() const;
    static void showChunks(world::LoadedWorld& world,
                           const std::function<std::vector<player::PlayerSession*>()>& players,
                           std::span<const std::int32_t> chunks);
    [[nodiscard]] blockUpdates::Target updateTarget() const;
    void tell(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) const;
    world::LoadedWorld* world_;
    std::shared_ptr<const ModelHooks> shared_;
    const ModelHooks& hooks_;
    std::function<std::vector<player::PlayerSession*>()> players_;
    ServerWorldEnv env_;
};

} // namespace basalt::server::runtime::plugins::worldModel
