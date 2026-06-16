#include "DataFix.hpp"
#include "WorldBlockUpdates.hpp"
#include "WorldItemUse.hpp"

#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/MapColors.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/VanillaTables.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/registry/VanillaConfigurationRegistries.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/maps/MapDecorationTypes.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/light/Light.hpp>
#include <basalt/wire/common/placeholder/PlaceholderText.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <utility>

namespace basalt::server::runtime::plugins {
namespace {

namespace reg = server::registry;

using server::plugins::PluginEntityRef;
using server::plugins::PluginHologramRef;
using server::plugins::PluginWorldRef;
using wire::common::Identifier;

constexpr std::string_view kBukkitValues = "PublicBukkitValues";

const Identifier& textDisplay() {
    static const Identifier kType = Identifier::vanilla("text_display");
    return kType;
}

class WorldEntityRef : public PluginEntityRef {
public:
    WorldEntityRef(world::LoadedWorld& world, std::int32_t entityId, std::string type)
        : world_(&world), entityId_(entityId), type_(std::move(type)) {}

    [[nodiscard]] std::int32_t id() const override { return entityId_; }

    [[nodiscard]] std::string type() const override {
        const entity::ServerEntity* found = entity();
        return found == nullptr ? type_ : found->type().toString();
    }

    [[nodiscard]] std::string world() const override { return world_->name(); }

    [[nodiscard]] double x() const override {
        const entity::ServerEntity* found = entity();
        return found == nullptr ? 0.0 : found->x;
    }

    [[nodiscard]] double y() const override {
        const entity::ServerEntity* found = entity();
        return found == nullptr ? 0.0 : found->y;
    }

    [[nodiscard]] double z() const override {
        const entity::ServerEntity* found = entity();
        return found == nullptr ? 0.0 : found->z;
    }

    [[nodiscard]] bool alive() const override { return entity() != nullptr; }

    [[nodiscard]] double health() const override {
        const entity::ServerEntity* found = entity();
        return found == nullptr ? 0.0 : found->health;
    }

    void setHealth(double value) override {
        entity::ServerEntity* found = entity();
        if (found != nullptr) {
            found->health = value;
        }
    }

    void remove() override {
        entity::ServerEntity* found = entity();
        if (found != nullptr) {
            found->removed = true;
        }
    }

    bool teleport(double x, double y, double z) override {
        entity::ServerEntity* found = entity();
        if (found == nullptr) {
            return false;
        }
        found->x = x;
        found->y = y;
        found->z = z;
        return true;
    }

    [[nodiscard]] std::string uuid() const override {
        const entity::ServerEntity* found = entity();
        return found == nullptr ? std::string{} : found->uuid().toString();
    }

    [[nodiscard]] std::array<float, 2> rotation() const override {
        const entity::ServerEntity* found = entity();
        return found == nullptr ? std::array<float, 2>{0.0F, 0.0F} : std::array<float, 2>{found->yaw, found->pitch};
    }

    bool setRotation(float yaw, float pitch) override {
        entity::ServerEntity* found = entity();
        if (found == nullptr) {
            return false;
        }
        found->yaw = yaw;
        found->pitch = pitch;
        return true;
    }

    [[nodiscard]] std::array<double, 3> velocity() const override {
        const entity::ServerEntity* found = entity();
        return found == nullptr ? std::array<double, 3>{0.0, 0.0, 0.0}
                                : std::array<double, 3>{found->velocityX, found->velocityY, found->velocityZ};
    }

    bool setVelocity(double x, double y, double z) override {
        entity::ServerEntity* found = entity();
        if (found == nullptr) {
            return false;
        }
        found->velocityX = x;
        found->velocityY = y;
        found->velocityZ = z;
        return true;
    }

    [[nodiscard]] bool onGround() const override {
        const entity::ServerEntity* found = entity();
        return found != nullptr && found->onGround;
    }

    [[nodiscard]] std::int32_t age() const override {
        const entity::ServerEntity* found = entity();
        return found == nullptr ? 0 : found->age;
    }

    [[nodiscard]] double width() const override {
        const entity::ServerEntity* found = entity();
        return found == nullptr ? 0.6 : reg::entityTypeShapes::widthOf(found->type());
    }

    [[nodiscard]] double height() const override {
        const entity::ServerEntity* found = entity();
        return found == nullptr ? 1.8 : reg::entityTypeShapes::heightOf(found->type());
    }

    [[nodiscard]] std::optional<std::string> customName() const override {
        const entity::ServerEntity* found = entity();
        return found == nullptr ? std::nullopt : found->customName;
    }

protected:
    [[nodiscard]] entity::ServerEntity* entity() const {
        entity::ServerEntity* found = world_->state().entity(entityId_);
        return found != nullptr && !found->removed ? found : nullptr;
    }

    world::LoadedWorld* world_;

private:
    std::int32_t entityId_;
    std::string type_;
};

class HologramRef final : public PluginHologramRef {
public:
    HologramRef(world::LoadedWorld& world, std::int32_t entityId, std::string type)
        : inner_(world, entityId, std::move(type)) {}

    [[nodiscard]] std::int32_t id() const override { return inner_.id(); }

    [[nodiscard]] std::string type() const override { return inner_.type(); }

    [[nodiscard]] std::string world() const override { return inner_.world(); }

    [[nodiscard]] double x() const override { return inner_.x(); }

    [[nodiscard]] double y() const override { return inner_.y(); }

    [[nodiscard]] double z() const override { return inner_.z(); }

    [[nodiscard]] bool alive() const override { return inner_.alive(); }

    [[nodiscard]] double health() const override { return inner_.health(); }

    void setHealth(double value) override { inner_.setHealth(value); }

    void remove() override { inner_.remove(); }

    bool teleport(double x, double y, double z) override { return inner_.teleport(x, y, z); }

    [[nodiscard]] std::string text() const override {
        const entity::ServerEntity* found = held();
        return found == nullptr ? "" : found->displayText.value_or("");
    }

    void setText(std::string_view value) override {
        entity::ServerEntity* found = held();
        if (found != nullptr) {
            found->displayText = std::string{value};
        }
    }

    void showOnlyTo(const std::vector<std::string>& uuids) override {
        entity::ServerEntity* found = held();
        if (found != nullptr) {
            found->visibleTo = uuids;
        }
    }

    void showToAll() override {
        entity::ServerEntity* found = held();
        if (found != nullptr) {
            found->visibleTo.reset();
        }
    }

    [[nodiscard]] std::optional<std::vector<std::string>> audience() const override {
        const entity::ServerEntity* found = held();
        return found == nullptr ? std::nullopt : found->visibleTo;
    }

private:
    class Access : public WorldEntityRef {
    public:
        using WorldEntityRef::entity;
        using WorldEntityRef::WorldEntityRef;
    };

    [[nodiscard]] entity::ServerEntity* held() const { return inner_.entity(); }

    mutable Access inner_;
};

class LoadedWorldRef final : public PluginWorldRef {
public:
    LoadedWorldRef(world::LoadedWorld& world, ModelHooks hooks,
                   std::function<std::vector<player::PlayerSession*>()> players)
        : world_(&world), hooks_(std::move(hooks)), players_(std::move(players)) {}

    [[nodiscard]] std::string name() const override { return world_->name(); }

    [[nodiscard]] std::int64_t time() const override { return world_->clock().totalTicks(); }

    [[nodiscard]] bool raining() const override { return world_->weather().raining; }

    [[nodiscard]] bool thundering() const override { return world_->weather().thundering; }

    [[nodiscard]] std::string difficulty() const override { return world_->settings().difficulty; }

    [[nodiscard]] std::optional<std::string> gameRule(std::string_view name) const override {
        const world::GameRule* rule = world::gameRules::byId(name);
        return rule == nullptr ? std::nullopt : std::optional{world_->gameRules().serialized(*rule)};
    }

    bool setGameRule(std::string_view name, std::string_view value) override {
        const world::GameRule* rule = world::gameRules::byId(name);
        if (rule == nullptr) {
            return false;
        }
        if (rule->kind == world::GameRule::Kind::Boolean) {
            if (value != "true" && value != "false") {
                return false;
            }
        } else {
            std::int32_t number = 0;
            const auto* end = value.data() + value.size();
            if (std::from_chars(value.data(), end, number).ptr != end) {
                return false;
            }
        }
        world_->gameRules().set(*rule, std::string{value});
        return true;
    }

    [[nodiscard]] std::vector<std::string> gameRules() const override {
        std::vector<std::string> ids;
        for (const world::GameRule& rule : world::gameRules::all()) {
            ids.emplace_back(rule.id);
        }
        return ids;
    }

    [[nodiscard]] std::optional<std::string> block(std::int32_t x, std::int32_t y, std::int32_t z) const override {
        const std::optional<std::int32_t> stateId = world_->state().blockState(x, y, z);
        const Identifier* block = stateId.has_value() ? reg::blockStateDefinitions::blockOf(*stateId) : nullptr;
        return block == nullptr ? std::nullopt : std::optional{block->toString()};
    }

    [[nodiscard]] std::optional<std::string> blockState(std::int32_t x, std::int32_t y, std::int32_t z) const override {
        const std::optional<std::int32_t> stateId = world_->state().blockState(x, y, z);
        const Identifier* block = stateId.has_value() ? reg::blockStateDefinitions::blockOf(*stateId) : nullptr;
        if (block == nullptr) {
            return std::nullopt;
        }
        std::string text = block->toString();
        const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(*stateId);
        char separator = '[';
        for (const auto& [name, value] : properties) {
            text += separator;
            text += name + "=" + value;
            separator = ',';
        }
        return properties.empty() ? text : text + "]";
    }

    [[nodiscard]] static std::optional<std::int32_t> parseState(std::string_view text) {
        std::map<std::string, std::string> properties;
        const std::size_t open = text.find('[');
        if (open != std::string_view::npos) {
            if (!text.ends_with(']')) {
                return std::nullopt;
            }
            std::string_view list = text.substr(open + 1, text.size() - open - 2);
            while (!list.empty()) {
                const std::size_t comma = list.find(',');
                const std::string_view pair = list.substr(0, comma);
                const std::size_t equals = pair.find('=');
                if (equals == std::string_view::npos || equals == 0 || equals + 1 == pair.size()) {
                    return std::nullopt;
                }
                properties[std::string{pair.substr(0, equals)}] = std::string{pair.substr(equals + 1)};
                list = comma == std::string_view::npos ? std::string_view{} : list.substr(comma + 1);
            }
            text = text.substr(0, open);
        }
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(std::string{text});
        if (!id.has_value()) {
            return std::nullopt;
        }
        const std::optional<std::int32_t> base = reg::blocks::defaultStateIdOf(*id);
        if (!base.has_value() || properties.empty()) {
            return base;
        }
        return reg::blockStateDefinitions::stateIdOf(*id, properties, base);
    }

    bool setBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view block) override {
        const std::optional<std::int32_t> stateId = parseState(block);
        if (!stateId.has_value() || !world_->state().setBlockState(x, y, z, *stateId)) {
            return false;
        }
        // Told to everyone in this world: a block only the server knows about is invisible, clients never ask again.
        if (players_) {
            for (const player::PlayerSession* player : players_()) {
                if (player->worldName == world_->name()) {
                    player::sendBlockChangeTo(*player, x, y, z, *stateId);
                }
            }
        }
        return true;
    }

    [[nodiscard]] std::string dimension() const override {
        return "minecraft:" + std::string{config::dimensionName(world_->declaration().dimension)};
    }

    [[nodiscard]] std::int64_t seed() const override { return world_->metadata().seed; }

    [[nodiscard]] std::int32_t minHeight() const override { return chunkHeight().minY(); }

    [[nodiscard]] std::int32_t height() const override { return chunkHeight().height(); }

    [[nodiscard]] std::array<std::int32_t, 3> spawn() const override {
        const std::array<double, 3>& at = world_->spawn();
        return {static_cast<std::int32_t>(std::floor(at[0])), static_cast<std::int32_t>(std::floor(at[1])),
                static_cast<std::int32_t>(std::floor(at[2]))};
    }

    void setSpawn(std::int32_t x, std::int32_t y, std::int32_t z) override { world_->setSpawn(x, y, z); }

    [[nodiscard]] std::int64_t dayTime() const override { return world_->clock().totalTicks(); }

    void setDayTime(std::int64_t ticks) override {
        world_->clock().setTotalTicks(ticks);
        if (hooks_.environment) {
            hooks_.environment().syncTime(*world_);
        }
    }

    void setWeather(bool raining, bool thundering) override {
        world_->weather().raining = raining;
        world_->weather().thundering = thundering;
        if (hooks_.environment) {
            hooks_.environment().syncWeather(*world_);
        }
    }

    bool setDifficulty(std::string_view name) override {
        if (name != "peaceful" && name != "easy" && name != "normal" && name != "hard") {
            return false;
        }
        world_->setDifficulty(std::string{name});
        return true;
    }

    [[nodiscard]] std::int32_t stateAt(std::int32_t x, std::int32_t y, std::int32_t z, bool load) override {
        if (load) {
            ensureChunk(x >> 4, z >> 4);
        }
        return world_->state().blockState(x, y, z).value_or(-1);
    }

    bool setStateAt(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, bool notify) override {
        ensureChunk(x >> 4, z >> 4);
        if (!world_->state().setBlockState(x, y, z, stateId)) {
            return false;
        }
        if (notify) {
            tell(x, y, z, stateId);
        }
        return true;
    }

    std::int32_t readStates(std::span<const std::int32_t> positions, std::span<std::int32_t> out, bool load) override {
        const std::size_t count = std::min(positions.size() / 3, out.size());
        std::int32_t found = 0;
        for (std::size_t index = 0; index < count; ++index) {
            const std::int32_t x = positions[index * 3];
            const std::int32_t y = positions[index * 3 + 1];
            const std::int32_t z = positions[index * 3 + 2];
            if (load) {
                ensureChunk(x >> 4, z >> 4);
            }
            out[index] = world_->state().blockState(x, y, z).value_or(-1);
            found += out[index] >= 0 ? 1 : 0;
        }
        return found;
    }

    std::int32_t writeStates(std::span<const std::int32_t> positions, std::span<const std::int32_t> states,
                             bool notify) override {
        const std::size_t count = std::min(positions.size() / 3, states.size());
        std::int32_t changed = 0;
        for (std::size_t index = 0; index < count; ++index) {
            changed += setStateAt(positions[index * 3], positions[index * 3 + 1], positions[index * 3 + 2],
                                  states[index], notify)
                           ? 1
                           : 0;
        }
        return changed;
    }

    bool readSection(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ, bool load,
                     std::span<std::int32_t> out) override {
        if (load) {
            ensureChunk(chunkX, chunkZ);
        }
        return world_->state().copySection(chunkX, sectionY, chunkZ, out);
    }

    bool readSections(std::int32_t chunkX, std::int32_t chunkZ, bool load, std::span<std::int32_t> out) override {
        if (load) {
            ensureChunk(chunkX, chunkZ);
        }
        return world_->state().copySections(chunkX, chunkZ, out);
    }

    [[nodiscard]] std::int32_t chunkStatus(std::int32_t chunkX, std::int32_t chunkZ) const override {
        constexpr std::int32_t kSimulationRadius = 10;
        if (!world_->state().isLoaded(chunkX, chunkZ)) {
            return world_->generationQueue().pendingKeys().contains(world::ChunkGenerationQueue::pack(chunkX, chunkZ))
                       ? 1
                       : 0;
        }
        if (players_) {
            for (const player::PlayerSession* player : players_()) {
                if (player->worldName == world_->name() &&
                    std::abs((static_cast<std::int32_t>(std::floor(player->x)) >> 4) - chunkX) <= kSimulationRadius &&
                    std::abs((static_cast<std::int32_t>(std::floor(player->z)) >> 4) - chunkZ) <= kSimulationRadius) {
                    return 3;
                }
            }
        }
        return 2;
    }

    void requestChunk(std::int32_t chunkX, std::int32_t chunkZ, bool generate, std::int32_t priority,
                      std::function<void(bool)> done) override {
        if (world_->state().isLoaded(chunkX, chunkZ)) {
            done(true);
            return;
        }
        if (!generate) {
            done(false);
            return;
        }
        // Held while it is awaited, since the sweep drops requests nobody stands near.
        const std::string token = "request:" + std::to_string(chunkX) + "," + std::to_string(chunkZ);
        world_->holdChunks(token, {world::ChunkGenerationQueue::pack(chunkX, chunkZ)});
        world_->generationQueue().request(chunkX, chunkZ, priority);
        awaitChunk(chunkX, chunkZ, 0,
                   std::make_shared<std::function<void(bool)>>([this, token, done = std::move(done)](bool loaded) {
                       done(loaded);
                       world_->releaseChunks(token);
                   }));
    }

    bool holdChunks(std::string_view token, std::span<const std::int32_t> chunks) override {
        std::vector<std::int64_t> keys;
        for (std::size_t at = 0; at + 1 < chunks.size(); at += 2) {
            keys.push_back(world::ChunkGenerationQueue::pack(chunks[at], chunks[at + 1]));
        }
        world_->holdChunks(std::string{token}, keys);
        return true;
    }

    void releaseChunks(std::string_view token) override { world_->releaseChunks(std::string{token}); }

    std::int32_t settleBlocks(std::span<const std::int32_t> positions, std::span<const std::int32_t> previous,
                              std::uint32_t effects) override {
        constexpr std::uint32_t kUpdates = BlockEffects::kNeighbors | BlockEffects::kShapes | BlockEffects::kTicks |
                                           BlockEffects::kPlace | BlockEffects::kDeparture;
        std::int32_t reached = 0;
        if ((effects & kUpdates) != 0) {
            reached = blockUpdates::settle(updateTarget(), positions, previous, effects);
        }
        std::vector<std::int32_t> stale;
        for (std::size_t at = 0; at + 2 < positions.size(); at += 3) {
            if (!world_->state().isLoaded(positions[at] >> 4, positions[at + 2] >> 4)) {
                continue;
            }
            reached += (effects & kUpdates) == 0 ? 1 : 0;
            if ((effects & BlockEffects::kClients) != 0) {
                tell(positions[at], positions[at + 1], positions[at + 2],
                     world_->state().blockState(positions[at], positions[at + 1], positions[at + 2]).value_or(0));
            }
            if ((effects & BlockEffects::kLight) != 0) {
                stale.push_back(positions[at] >> 4);
                stale.push_back(positions[at + 2] >> 4);
            }
        }
        if (!stale.empty()) {
            relightChunks(stale);
        }
        return reached;
    }

    std::int32_t neighborChanged(std::span<const std::int32_t> positions,
                                 std::span<const std::int32_t> sources) override {
        return blockUpdates::neighbourChanged(updateTarget(), positions, sources);
    }

    [[nodiscard]] std::int32_t validState(std::int32_t x, std::int32_t y, std::int32_t z,
                                          std::int32_t stateId) override {
        return world_->state().isLoaded(x >> 4, z >> 4) ? blockUpdates::validState(updateTarget(), x, y, z, stateId)
                                                        : stateId;
    }

    std::int32_t updateShapes(std::span<const std::int32_t> positions, std::span<const std::int32_t> states,
                              bool indirect) override {
        return blockUpdates::shapes(updateTarget(), positions, states, indirect);
    }

    std::int32_t useItemOn(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t face, std::string_view item,
                           float yaw, float pitch) override {
        ensureChunk(x >> 4, z >> 4);
        return itemUse::placeBlockItem(
            updateTarget(), item, itemUse::Click{.x = x, .y = y, .z = z, .face = face, .yaw = yaw, .pitch = pitch});
    }

    bool scheduleTick(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t delay, bool fluid) override {
        const std::optional<std::int32_t> here = world_->state().blockState(x, y, z);
        if (!here.has_value()) {
            return false;
        }
        if (!fluid) {
            const Identifier* block = reg::blockStateDefinitions::blockOf(*here);
            if (block == nullptr || *here == reg::blocks::air().stateId) {
                return false;
            }
            world_->state().scheduleBlockTick(x, y, z, *block, delay);
            return true;
        }
        const std::optional<world::FluidKind> kind = world::fluidFlow::kindOf(*here);
        if (!kind.has_value()) {
            return false;
        }
        world_->state().scheduleFluidTick(x, y, z, world::fluidId(*kind, world::isSourceFluid(*here)), delay);
        return true;
    }

    std::int32_t applyChanges(std::span<const std::int32_t> positions, std::span<const std::int32_t> states,
                              std::uint32_t effects) override {
        using World = world::WorldState;
        const World::SideEffects spill =
            (effects & BlockEffects::kEntities) != 0 ? World::SideEffects::kRun : World::SideEffects::kSkip;
        const World::PoiTracking poi =
            (effects & BlockEffects::kPoi) != 0 ? World::PoiTracking::kTrack : World::PoiTracking::kSkip;
        const std::size_t count = std::min(positions.size() / 3, states.size());
        std::vector<std::int32_t> changed;
        std::vector<std::int32_t> before;
        for (std::size_t index = 0; index < count; ++index) {
            const std::int32_t x = positions[index * 3];
            const std::int32_t y = positions[index * 3 + 1];
            const std::int32_t z = positions[index * 3 + 2];
            ensureChunk(x >> 4, z >> 4);
            const std::int32_t was = world_->state().blockState(x, y, z).value_or(-1);
            if (was < 0 || was == states[index] || !world_->state().setBlockState(x, y, z, states[index], spill, poi)) {
                continue;
            }
            changed.insert(changed.end(), {x, y, z});
            before.push_back(was);
        }
        static_cast<void>(settleBlocks(changed, before, effects));
        return static_cast<std::int32_t>(changed.size() / 3);
    }

    bool writeSection(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ,
                      std::span<const std::int32_t> states, bool notify) override {
        const std::int32_t index = sectionY - chunkHeight().minSection();
        if (index < 0 || index >= chunkHeight().sectionCount() ||
            states.size() < server::world::ChunkSection::kBlocks) {
            return false;
        }
        ensureChunk(chunkX, chunkZ);
        std::array<std::int32_t, server::world::ChunkSection::kBlocks> before{};
        if (!readSection(chunkX, sectionY, chunkZ, false, before)) {
            return false;
        }
        for (std::int32_t at = 0; at < static_cast<std::int32_t>(before.size()); ++at) {
            if (before[static_cast<std::size_t>(at)] == states[static_cast<std::size_t>(at)]) {
                continue;
            }
            setStateAt((chunkX << 4) + (at & 15), (sectionY << 4) + (at >> 8), (chunkZ << 4) + ((at >> 4) & 15),
                       states[static_cast<std::size_t>(at)], notify);
        }
        return true;
    }

    std::int32_t applySections(std::int32_t chunkX, std::int32_t chunkZ, std::span<const std::int32_t> sectionYs,
                               std::span<const std::int32_t> states, bool resend,
                               std::span<std::int32_t> previous) override {
        const std::array<std::int32_t, 3> chunk{chunkX, chunkZ, static_cast<std::int32_t>(sectionYs.size())};
        return applyChunks(chunk, sectionYs, states, resend, previous);
    }

    std::int32_t applyChunks(std::span<const std::int32_t> chunks, std::span<const std::int32_t> sectionYs,
                             std::span<const std::int32_t> states, bool resend,
                             std::span<std::int32_t> previous) override {
        constexpr std::size_t kBlocks = server::world::ChunkSection::kBlocks;
        constexpr std::size_t kBlockChangeLimit = 24;
        if (states.size() < sectionYs.size() * kBlocks) {
            return -1;
        }
        std::int32_t changed = 0;
        bool failed = false;
        std::size_t section = 0;
        std::vector<std::int32_t> resent;
        std::vector<std::uint16_t> offsets;
        std::vector<std::uint16_t> chunkOffsets;
        std::vector<std::int32_t> chunkStates;
        for (std::size_t at = 0; at + 2 < chunks.size(); at += 3) {
            const std::int32_t chunkX = chunks[at];
            const std::int32_t chunkZ = chunks[at + 1];
            const std::size_t count = static_cast<std::size_t>(std::max(chunks[at + 2], 0));
            if (section + count > sectionYs.size()) {
                return -1;
            }
            ensureChunk(chunkX, chunkZ);
            std::int32_t inChunk = 0;
            chunkOffsets.clear();
            chunkStates.clear();
            for (std::size_t index = 0; index < count; ++index, ++section) {
                std::span<std::int32_t> held;
                if (previous.size() >= (section + 1) * kBlocks) {
                    held = previous.subspan(section * kBlocks, kBlocks);
                }
                offsets.clear();
                const std::span<const std::int32_t> next = states.subspan(section * kBlocks, kBlocks);
                const std::int32_t done =
                    world_->state().replaceSection(chunkX, sectionYs[section], chunkZ, next, held, &offsets);
                if (done < 0) {
                    failed = true;
                    continue;
                }
                inChunk += done;
                if (inChunk <= static_cast<std::int32_t>(kBlockChangeLimit)) {
                    for (const std::uint16_t offset : offsets) {
                        chunkOffsets.push_back(static_cast<std::uint16_t>(offset));
                        chunkStates.push_back(sectionYs[section]);
                        chunkStates.push_back(next[offset]);
                    }
                }
            }
            changed += inChunk;
            if (inChunk == 0 || !resend) {
                continue;
            }
            if (inChunk <= static_cast<std::int32_t>(kBlockChangeLimit)) {
                for (std::size_t one = 0; one < chunkOffsets.size(); ++one) {
                    const std::uint16_t offset = chunkOffsets[one];
                    tell((chunkX << 4) + (offset & 15), (chunkStates[one * 2] << 4) + (offset >> 8),
                         (chunkZ << 4) + ((offset >> 4) & 15), chunkStates[one * 2 + 1]);
                }
            } else {
                resent.push_back(chunkX);
                resent.push_back(chunkZ);
            }
        }
        if (!resent.empty()) {
            resendChunks(resent);
        }
        return failed ? -1 : changed;
    }

    [[nodiscard]] std::uint64_t blockVersion() const override { return world_->state().blockVersion(); }

    void tellBlockEntity(std::int32_t x, std::int32_t y, std::int32_t z, const server::world::ChunkBlockEntity& made) {
        const Identifier* entityType = reg::blockEntityTypes::ofBlock(made.type());
        const std::optional<std::int32_t> typeId =
            reg::blockEntityTypes::idOf(entityType != nullptr ? *entityType : made.type());
        if (!typeId.has_value() || !players_) {
            return;
        }
        const wire::protocol::play::BlockEntityDataPacket packet{
            .pos = {x, y, z}, .typeId = *typeId, .tag = made.data()};
        for (const player::PlayerSession* player : players_()) {
            if (player->worldName == world_->name() && player->connection().sendPlayPacket) {
                player->connection().sendPlayPacket(wire::protocol::play::BlockEntityDataPacket::kId,
                                                    [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
        }
    }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> blockEntityNbt(std::int32_t x, std::int32_t y,
                                                                          std::int32_t z) override {
        const server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
        const server::world::ChunkBlockEntity* held =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
        if (held == nullptr) {
            return std::nullopt;
        }
        wire::nbt::NbtCompound tag = held->data();
        const Identifier* entityType = reg::blockEntityTypes::ofBlock(held->type());
        tag.putString("id", (entityType != nullptr ? *entityType : held->type()).toString());
        tag.putInt("x", x);
        tag.putInt("y", y);
        tag.putInt("z", z);
        return wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(std::move(tag)));
    }

    [[nodiscard]] std::vector<std::int32_t> blockEntityPositions(std::int32_t chunkX, std::int32_t chunkZ) override {
        std::vector<std::int32_t> found;
        const server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
        if (chunk == nullptr) {
            return found;
        }
        for (const server::world::ChunkBlockEntity& held : chunk->blockEntities()) {
            found.push_back((chunkX << 4) + held.localX());
            found.push_back(held.y());
            found.push_back((chunkZ << 4) + held.localZ());
        }
        return found;
    }

    bool setBlockEntityNbt(std::int32_t x, std::int32_t y, std::int32_t z, std::span<const std::uint8_t> nbt) override {
        server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
        const server::world::ChunkBlockEntity* held =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
        const auto parsed = wire::nbt::nbtIO::read(nbt);
        const wire::nbt::NbtCompound* given = parsed.has_value() ? parsed->tag.get<wire::nbt::NbtCompound>() : nullptr;
        if (held == nullptr || given == nullptr) {
            return false;
        }
        wire::nbt::NbtCompound data = *given;
        for (const char* key : {"id", "x", "y", "z"}) {
            data.remove(key);
        }
        server::world::ChunkBlockEntity next = held->withData(std::move(data));
        chunk->setBlockEntity(next);
        tellBlockEntity(x, y, z, next);
        return true;
    }

    bool createBlockEntity(std::int32_t x, std::int32_t y, std::int32_t z, std::span<const std::uint8_t> nbt) override {
        server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
        const std::int32_t stateId = chunk == nullptr ? -1 : world_->state().blockState(x, y, z).value_or(-1);
        const Identifier* block = stateId < 0 ? nullptr : reg::blockStateDefinitions::blockOf(stateId);
        if (block == nullptr || reg::blockEntityTypes::ofBlock(*block) == nullptr) {
            return false;
        }
        wire::nbt::NbtCompound data = world::blockEntityDefaults::of(*block, stateId);
        if (!nbt.empty()) {
            const auto parsed = wire::nbt::nbtIO::read(nbt);
            const wire::nbt::NbtCompound* given =
                parsed.has_value() ? parsed->tag.get<wire::nbt::NbtCompound>() : nullptr;
            if (given == nullptr) {
                return false;
            }
            data = *given;
            for (const char* key : {"id", "x", "y", "z"}) {
                data.remove(key);
            }
        }
        const server::world::ChunkBlockEntity made{x & 15, y, z & 15, *block, std::move(data)};
        chunk->setBlockEntity(made);
        tellBlockEntity(x, y, z, made);
        return true;
    }

    bool removeBlockEntity(std::int32_t x, std::int32_t y, std::int32_t z) override {
        server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
        if (chunk == nullptr || chunk->blockEntityAt(x & 15, y, z & 15) == nullptr) {
            return false;
        }
        chunk->removeBlockEntityAt(x & 15, y, z & 15);
        return true;
    }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> entityNbt(std::int32_t entityId) override {
        const entity::ServerEntity* found = world_->state().entity(entityId);
        if (found == nullptr) {
            return std::nullopt;
        }
        return wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(entity::entityNbt::encode(*found)));
    }

    std::int32_t spawnEntityNbt(std::span<const std::uint8_t> nbt, std::string_view cause) override {
        const auto parsed = wire::nbt::nbtIO::read(nbt);
        const wire::nbt::NbtCompound* given = parsed.has_value() ? parsed->tag.get<wire::nbt::NbtCompound>() : nullptr;
        if (given == nullptr || !hooks_.nextEntityId) {
            return -1;
        }
        if (hooks_.spawnAllowed) {
            const auto type = Identifier::parse(std::string{given->getString("id")});
            std::array<double, 3> at{};
            if (const wire::nbt::NbtList* pos = given->getList("Pos"); pos != nullptr && pos->size() >= 3) {
                for (std::size_t axis = 0; axis < 3; ++axis) {
                    if (const double* value = (*pos)[axis].get<double>(); value != nullptr) {
                        at[axis] = *value;
                    }
                }
            }
            if (type.has_value() && !hooks_.spawnAllowed(*world_, *type, at[0], at[1], at[2], cause)) {
                return -1;
            }
        }
        const entity::entityNbt::AllocateEntityIdFor allocate = [this](const Identifier&) {
            return hooks_.nextEntityId();
        };
        const entity::entityNbt::ProtocolTypeOf protocolTypeOf =
            [](const Identifier& type) -> std::optional<std::int32_t> {
            const reg::EntityTypeEntry* entry = reg::entityTypes::of(type);
            return entry == nullptr ? std::nullopt : std::optional{entry->protocolId};
        };
        std::int32_t first = -1;
        for (std::unique_ptr<entity::ServerEntity>& restored :
             entity::entityNbt::decodeTree(*given, allocate, protocolTypeOf)) {
            const std::int32_t id = world_->state().spawnEntity(std::move(restored)).entityId();
            first = first < 0 ? id : first;
        }
        return first;
    }

    [[nodiscard]] std::optional<DragonFightFacts> dragonFight() override {
        const world::DragonFight* fight = world_->dragonFight();
        if (fight == nullptr) {
            return std::nullopt;
        }
        DragonFightFacts facts;
        facts.previouslyKilled = fight->previouslyKilled;
        facts.portal = fight->exitPortalLocation;
        if (fight->respawnStage.has_value()) {
            facts.respawnStage = std::string{world::respawnStageName(*fight->respawnStage)};
        }
        facts.gatewaysLeft = static_cast<std::int32_t>(fight->gateways.size());
        if (fight->dragonUuid.has_value()) {
            facts.dragonUuid = fight->dragonUuid->toString();
        }
        return facts;
    }

    bool setDragonPreviouslyKilled(bool value) override {
        world::DragonFight* fight = world_->dragonFight();
        if (fight == nullptr) {
            return false;
        }
        fight->previouslyKilled = value;
        return true;
    }

    [[nodiscard]] std::optional<std::array<std::int32_t, 2>> locateStructure(std::string_view structure, std::int32_t x,
                                                                             std::int32_t z, std::int32_t cells,
                                                                             bool skipExisting) override {
        static_cast<void>(skipExisting);
        const auto wanted = Identifier::parse(std::string{structure});
        if (!wanted.has_value()) {
            return std::nullopt;
        }
        const std::int64_t seed = world_->metadata().seed;
        if (*wanted == Identifier::vanilla("stronghold")) {
            server::world::terrain::StrongholdStructureGenerator* strongholds = world_->strongholdGenerator();
            const auto found = strongholds == nullptr ? std::nullopt : strongholds->locate(x, z);
            return found.has_value() ? std::optional{std::array{found->first * 16, found->second * 16}} : std::nullopt;
        }
        for (const world::StructureLocator& locator : world_->structureLocators()) {
            if (locator.id == *wanted) {
                const auto found = locator.locate(seed, x >> 4, z >> 4, cells, {});
                return found.has_value() ? std::optional{std::array{found->first * 16, found->second * 16}}
                                         : std::nullopt;
            }
        }
        if (server::world::terrain::VillageStructureGenerator* generator = world_->structureSetGenerator(*wanted);
            generator != nullptr) {
            const auto found = generator->locate(seed, x >> 4, z >> 4, &*wanted);
            return found.has_value() ? std::optional{std::array{found->chunkX * 16, found->chunkZ * 16}} : std::nullopt;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::array<std::int32_t, 3>> locateBiome(const std::vector<std::string>& biomes,
                                                                         std::int32_t x, std::int32_t y, std::int32_t z,
                                                                         std::int32_t radius,
                                                                         std::int32_t step) override {
        const server::world::terrain::OverworldBiomeSource* source = world_->biomeSource();
        if (source == nullptr || step <= 0) {
            return std::nullopt;
        }
        for (std::int32_t ring = 0; ring <= radius; ring += step) {
            for (std::int32_t dx = -ring; dx <= ring; dx += step) {
                for (std::int32_t dz = -ring; dz <= ring; dz += step) {
                    if (ring > 0 && dx * dx != ring * ring && dz * dz != ring * ring) {
                        continue;
                    }
                    const std::string_view name = source->biomeAt(x + dx, y, z + dz);
                    if (std::ranges::any_of(biomes, [name](const std::string& wanted) {
                            return wanted == name || (wanted.starts_with("minecraft:") && wanted.substr(10) == name);
                        })) {
                        return std::array{x + dx, y, z + dz};
                    }
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] static std::vector<std::uint8_t> packedData(const wire::nbt::NbtCompound& data) {
        return wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(data));
    }

    [[nodiscard]] static std::optional<wire::nbt::NbtCompound> unpackedData(std::span<const std::uint8_t> nbt) {
        const auto parsed = wire::nbt::nbtIO::read(nbt);
        const wire::nbt::NbtCompound* given = parsed.has_value() ? parsed->tag.get<wire::nbt::NbtCompound>() : nullptr;
        return given == nullptr ? std::nullopt : std::optional{*given};
    }

    [[nodiscard]] std::vector<std::uint8_t> pluginData() override { return packedData(world_->pluginData()); }

    bool setPluginData(std::span<const std::uint8_t> nbt) override {
        std::optional<wire::nbt::NbtCompound> given = unpackedData(nbt);
        if (!given.has_value()) {
            return false;
        }
        world_->setPluginData(std::move(*given));
        return true;
    }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> chunkPluginData(std::int32_t chunkX,
                                                                           std::int32_t chunkZ) override {
        const server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
        return chunk == nullptr ? std::nullopt : std::optional{packedData(chunk->pluginData())};
    }

    bool setChunkPluginData(std::int32_t chunkX, std::int32_t chunkZ, std::span<const std::uint8_t> nbt) override {
        server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
        std::optional<wire::nbt::NbtCompound> given = unpackedData(nbt);
        if (chunk == nullptr || !given.has_value()) {
            return false;
        }
        chunk->setPluginData(std::move(*given));
        return true;
    }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> blockEntityPluginData(std::int32_t x, std::int32_t y,
                                                                                 std::int32_t z) override {
        const server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
        const server::world::ChunkBlockEntity* held =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
        if (held == nullptr) {
            return std::nullopt;
        }
        const wire::nbt::NbtCompound* kept = held->data().getCompound(kBukkitValues);
        return packedData(kept == nullptr ? wire::nbt::NbtCompound{} : *kept);
    }

    bool setBlockEntityPluginData(std::int32_t x, std::int32_t y, std::int32_t z,
                                  std::span<const std::uint8_t> nbt) override {
        server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
        const server::world::ChunkBlockEntity* held =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
        std::optional<wire::nbt::NbtCompound> given = unpackedData(nbt);
        if (held == nullptr && chunk != nullptr && given.has_value() && !given->empty() &&
            createBlockEntity(x, y, z, {})) {
            held = chunk->blockEntityAt(x & 15, y, z & 15);
        }
        if (held == nullptr || !given.has_value()) {
            return false;
        }
        wire::nbt::NbtCompound data = held->data();
        if (given->empty()) {
            data.remove(kBukkitValues);
        } else {
            data.set(kBukkitValues, wire::nbt::NbtTag::ofCompound(std::move(*given)));
        }
        chunk->setBlockEntity(held->withData(std::move(data)));
        return true;
    }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> entityPluginData(std::int32_t entityId) override {
        const entity::ServerEntity* found = world_->state().entity(entityId);
        return found == nullptr ? std::nullopt : std::optional{packedData(found->pluginData)};
    }

    bool setEntityPluginData(std::int32_t entityId, std::span<const std::uint8_t> nbt) override {
        entity::ServerEntity* found = world_->state().entity(entityId);
        std::optional<wire::nbt::NbtCompound> given = unpackedData(nbt);
        if (found == nullptr || !given.has_value()) {
            return false;
        }
        found->pluginData = std::move(*given);
        return true;
    }

    bool readBiomes(std::int32_t chunkX, std::int32_t chunkZ, std::span<std::int32_t> out) override {
        const server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
        const std::int32_t cells = chunkHeight().height() / 4;
        if (chunk == nullptr || out.size() < static_cast<std::size_t>(cells) * 16) {
            return false;
        }
        for (std::int32_t qy = 0; qy < cells; ++qy) {
            for (std::int32_t qz = 0; qz < 4; ++qz) {
                for (std::int32_t qx = 0; qx < 4; ++qx) {
                    out[static_cast<std::size_t>((qy * 4 + qz) * 4 + qx)] = biomeIndexOf(chunk->biomeId(qx, qy, qz));
                }
            }
        }
        return true;
    }

    bool writeBiomes(std::int32_t chunkX, std::int32_t chunkZ, std::span<const std::int32_t> ids) override {
        server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
        const std::int32_t cells = chunkHeight().height() / 4;
        if (chunk == nullptr || ids.size() < static_cast<std::size_t>(cells) * 16) {
            return false;
        }
        for (std::int32_t qy = 0; qy < cells; ++qy) {
            for (std::int32_t qz = 0; qz < 4; ++qz) {
                for (std::int32_t qx = 0; qx < 4; ++qx) {
                    const std::int32_t id = ids[static_cast<std::size_t>((qy * 4 + qz) * 4 + qx)];
                    if (const std::optional<std::int32_t> biome = biomeIdAt(id); biome.has_value()) {
                        chunk->setBiomeId(qx, qy, qz, *biome);
                    }
                }
            }
        }
        const std::array<std::int32_t, 2> pair{chunkX, chunkZ};
        resendChunks(pair);
        return true;
    }

    void lightJob(std::span<const std::int32_t> chunks, std::function<void(std::int32_t)> done) override {
        std::vector<std::int64_t> keys;
        for (std::size_t at = 0; at + 1 < chunks.size(); at += 2) {
            keys.push_back(world::ChunkGenerationQueue::pack(chunks[at], chunks[at + 1]));
        }
        world_->relightAsync(std::move(keys), [world = world_, players = players_,
                                               pairs = std::vector<std::int32_t>(chunks.begin(), chunks.end()),
                                               done = std::move(done)](std::int32_t relit) {
            showChunks(*world, players, pairs);
            done(relit);
        });
    }

    [[nodiscard]] std::int32_t lightAt(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t layer) override {
        if (!world_->state().isLoaded(x >> 4, z >> 4)) {
            return -1;
        }
        const bool sky = layer == 1;
        world::worldLight::refresh(world_->state(), x >> 4, z >> 4, hasSkylight());
        return sky ? world::worldLight::skyLight(world_->state(), x, y, z, hasSkylight())
                   : world::worldLight::blockLight(world_->state(), x, y, z);
    }

    bool readLight(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ, std::int32_t layer, bool load,
                   std::span<std::uint8_t> out) override {
        const bool sky = layer == 1;
        if (load) {
            ensureChunk(chunkX, chunkZ);
        }
        // parity: the engine answers with what it has worked out so far; the light thread catches up with changes.
        if (out.size() < kLightBytes || (sky && !hasSkylight())) {
            return false;
        }
        const server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
        const server::world::Chunk::SharedLight cached =
            chunk == nullptr ? nullptr : (sky ? chunk->cachedSkyLight() : chunk->cachedBlockLight());
        const std::int32_t index = sectionY - chunkHeight().minSection();
        if (cached == nullptr || index < 0 || index >= chunkHeight().sectionCount()) {
            return false;
        }
        // A layer with nothing lit in it is an empty list, and a section with nothing in it an empty array.
        std::ranges::fill(out.first(kLightBytes), std::uint8_t{0});
        if (static_cast<std::size_t>(index) < cached->size()) {
            const std::vector<std::uint8_t>& bytes = (*cached)[static_cast<std::size_t>(index)];
            std::copy_n(bytes.begin(), std::min(bytes.size(), kLightBytes), out.begin());
        }
        return true;
    }

    bool writeLight(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ, std::int32_t layer,
                    std::span<const std::uint8_t> nibbles) override {
        const bool sky = layer == 1;
        if (nibbles.size() < kLightBytes || (sky && !hasSkylight())) {
            return false;
        }
        server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
        const std::int32_t index = sectionY - chunkHeight().minSection();
        if (chunk == nullptr || index < 0 ||
            !chunk->storeLightSection(sky, static_cast<std::size_t>(index), nibbles.first(kLightBytes))) {
            return false;
        }
        // The engine owns the light: what a plugin wrote is read back until things settle, then worked out again.
        constexpr std::chrono::milliseconds kQuiet{250};
        world_->relightSoon(chunkX, chunkZ, kQuiet,
                            [world = world_, players = players_, pair = std::array{chunkX, chunkZ}] {
                                showChunks(*world, players, pair);
                            });
        return true;
    }

    void checkLight(std::span<const std::int32_t> positions) override {
        for (std::size_t at = 0; at + 2 < positions.size(); at += 3) {
            world_->state().markLightAround(positions[at] >> 4, positions[at + 2] >> 4);
        }
    }

    void resendChunks(std::span<const std::int32_t> chunks) override {
        std::vector<std::int32_t> ready;
        std::vector<std::int32_t> unlit;
        std::vector<std::int64_t> keys;
        for (std::size_t at = 0; at + 1 < chunks.size(); at += 2) {
            const server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunks[at], chunks[at + 1]);
            if (chunk == nullptr) {
                continue;
            }
            const bool lit = chunk->cachedBlockLight() != nullptr &&
                             (!hasSkylight() || chunk->cachedSkyLight() != nullptr) && !chunk->lightIsStale();
            std::vector<std::int32_t>& into = lit ? ready : unlit;
            into.push_back(chunks[at]);
            into.push_back(chunks[at + 1]);
            if (!lit) {
                keys.push_back(world::ChunkGenerationQueue::pack(chunks[at], chunks[at + 1]));
            }
        }
        showChunks(*world_, players_, ready);
        if (!unlit.empty()) {
            world_->relightAsync(std::move(keys), [world = world_, players = players_, pairs = std::move(unlit)](
                                                      std::int32_t) { showChunks(*world, players, pairs); });
        }
    }

    [[nodiscard]] std::optional<std::string> biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const override {
        const server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
        const std::int32_t quartY = (y - chunkHeight().minY()) >> 2;
        if (chunk == nullptr || quartY < 0 || quartY >= chunkHeight().height() / 4) {
            return std::nullopt;
        }
        const std::int32_t id = chunk->biomeId((x & 15) >> 2, quartY, (z & 15) >> 2);
        for (const auto& [biomeId, name] : reg::biomeIdsTable()) {
            if (biomeId == id) {
                return "minecraft:" + std::string{name};
            }
        }
        return std::nullopt;
    }

    bool setBiomeAt(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view biome) override {
        server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
        const std::int32_t quartY = (y - chunkHeight().minY()) >> 2;
        if (chunk == nullptr || quartY < 0 || quartY >= chunkHeight().height() / 4) {
            return false;
        }
        const std::string_view path = biome.starts_with("minecraft:") ? biome.substr(10) : biome;
        for (const auto& [biomeId, name] : reg::biomeIdsTable()) {
            if (name == path) {
                chunk->setBiomeId((x & 15) >> 2, quartY, (z & 15) >> 2, biomeId);
                if (hooks_.environment) {
                    hooks_.environment().syncBiomes(*world_, x, z, x, z);
                }
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] std::int32_t highest(std::int32_t x, std::int32_t z, std::string_view kind, bool load) override {
        if (load) {
            ensureChunk(x >> 4, z >> 4);
        }
        const server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
        if (chunk == nullptr) {
            return chunkHeight().minY() - 1;
        }
        server::world::HeightmapType type = server::world::HeightmapType::MotionBlocking;
        if (kind == "surface") {
            type = server::world::HeightmapType::WorldSurface;
        } else if (kind == "ocean_floor") {
            type = server::world::HeightmapType::OceanFloor;
        }
        return chunk->heightAt(x & 15, z & 15, type) - 1;
    }

    [[nodiscard]] bool chunkLoaded(std::int32_t chunkX, std::int32_t chunkZ) const override {
        return world_->state().isLoaded(chunkX, chunkZ);
    }

    bool loadChunk(std::int32_t chunkX, std::int32_t chunkZ, bool generate) override {
        if (world_->state().isLoaded(chunkX, chunkZ)) {
            return true;
        }
        if (!generate) {
            return false;
        }
        ensureChunk(chunkX, chunkZ);
        return world_->state().isLoaded(chunkX, chunkZ);
    }

    bool unloadChunk(std::int32_t chunkX, std::int32_t chunkZ) override {
        server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
        if (chunk == nullptr || world_->isHeld(chunkX, chunkZ)) {
            return false;
        }
        const std::int32_t reach = world_->settings().viewDistance + 1;
        if (players_) {
            for (const player::PlayerSession* player : players_()) {
                if (player->worldName == world_->name() &&
                    std::abs(static_cast<std::int32_t>(std::floor(player->x)) / 16 - chunkX) <= reach &&
                    std::abs(static_cast<std::int32_t>(std::floor(player->z)) / 16 - chunkZ) <= reach) {
                    return false;
                }
            }
        }
        world_->save({chunk});
        return world_->state().evictClean({chunk}) > 0;
    }

    [[nodiscard]] std::vector<std::int32_t> loadedChunks() const override {
        std::vector<std::int32_t> flat;
        for (const server::world::Chunk* chunk : world_->state().chunks()) {
            flat.push_back(chunk->chunkX());
            flat.push_back(chunk->chunkZ());
        }
        return flat;
    }

    [[nodiscard]] std::shared_ptr<PluginEntityRef> entityById(std::int32_t id) const override {
        const entity::ServerEntity* found = world_->state().entity(id);
        if (found == nullptr || found->removed) {
            return nullptr;
        }
        return std::make_shared<WorldEntityRef>(*world_, id, found->type().toString());
    }

    [[nodiscard]] std::shared_ptr<PluginEntityRef> entityByUuid(std::string_view uuid) const override {
        for (const entity::ServerEntity* alive : world_->state().entities()) {
            if (!alive->removed && alive->uuid().toString() == uuid) {
                return std::make_shared<WorldEntityRef>(*world_, alive->entityId(), alive->type().toString());
            }
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<std::shared_ptr<PluginEntityRef>> entitiesIn(std::int32_t chunkX,
                                                                           std::int32_t chunkZ) const override {
        std::vector<std::shared_ptr<PluginEntityRef>> refs;
        for (const entity::ServerEntity* alive : world_->state().entitiesIn(chunkX, chunkZ)) {
            if (!alive->removed) {
                refs.push_back(std::make_shared<WorldEntityRef>(*world_, alive->entityId(), alive->type().toString()));
            }
        }
        return refs;
    }

    void save() override { world_->saveDirty(); }

    [[nodiscard]] std::string folder() const override { return world_->name(); }

    [[nodiscard]] std::vector<std::shared_ptr<PluginEntityRef>> entities() const override {
        std::vector<std::shared_ptr<PluginEntityRef>> refs;
        for (const entity::ServerEntity* alive : world_->state().entities()) {
            if (!alive->removed) {
                refs.push_back(std::make_shared<WorldEntityRef>(*world_, alive->entityId(), alive->type().toString()));
            }
        }
        return refs;
    }

    std::shared_ptr<PluginEntityRef> spawn(std::string_view type, double x, double y, double z) override {
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(std::string{type});
        if (!id.has_value() || reg::entityTypes::of(*id) == nullptr || !hooks_.spawnMob) {
            return nullptr;
        }
        entity::ServerEntity* spawned = hooks_.spawnMob(*world_, *id, x, y, z);
        if (spawned == nullptr) {
            return nullptr;
        }
        return std::make_shared<WorldEntityRef>(*world_, spawned->entityId(), spawned->type().toString());
    }

    std::shared_ptr<PluginHologramRef> hologram(std::string_view text, double x, double y, double z) override {
        if (!hooks_.spawnMob) {
            return nullptr;
        }
        entity::ServerEntity* spawned = hooks_.spawnMob(*world_, textDisplay(), x, y, z);
        if (spawned == nullptr) {
            return nullptr;
        }
        spawned->displayText = std::string{text};
        return std::make_shared<HologramRef>(*world_, spawned->entityId(), spawned->type().toString());
    }

    std::shared_ptr<server::plugins::PluginNpcRef> npc(std::string_view name, double x, double y, double z,
                                                       std::function<void(const std::string&)> onClick) override {
        if (hooks_.npcs == nullptr || !hooks_.nextEntityId) {
            return nullptr;
        }
        // Nameless on purpose: what is read over its head is the display made below.
        auto npc = std::make_shared<Npc>(hooks_.nextEntityId(), wire::common::Uuid::random(), std::string{},
                                         world_->name(), x, y, z);
        hooks_.npcs->add(npc, std::move(onClick));
        std::shared_ptr<PluginHologramRef> label;
        if (!name.empty()) {
            label = hologram(name, x, y + NpcHandle::kLabelHeight, z);
        }
        const std::function<std::vector<player::PlayerSession*>()> players = players_;
        return std::make_shared<NpcHandle>(
            std::move(npc), *hooks_.npcs,
            [players](
                const std::string& playerName) -> std::optional<std::pair<std::string, std::optional<std::string>>> {
                for (const player::PlayerSession* session : players()) {
                    if (session->name().size() != playerName.size() ||
                        !std::ranges::equal(session->name(), playerName, [](char left, char right) {
                            return std::tolower(left) == std::tolower(right);
                        })) {
                        continue;
                    }
                    for (const wire::protocol::login::GameProfileProperty& property : session->profileProperties) {
                        if (property.name == "textures") {
                            return std::pair{property.value, property.signature};
                        }
                    }
                }
                return std::nullopt;
            },
            std::move(label));
    }

private:
    [[nodiscard]] server::world::ChunkHeight chunkHeight() const {
        return config::dimensionFacts(world_->declaration().dimension).chunkHeight;
    }

    [[nodiscard]] static std::int32_t biomeIndexOf(std::int32_t biomeId) {
        std::int32_t index = 0;
        for (const auto& [id, name] : reg::biomeIdsTable()) {
            if (id == biomeId) {
                return index;
            }
            ++index;
        }
        return -1;
    }

    [[nodiscard]] static std::optional<std::int32_t> biomeIdAt(std::int32_t index) {
        std::int32_t at = 0;
        for (const auto& [id, name] : reg::biomeIdsTable()) {
            if (at++ == index) {
                return id;
            }
        }
        return std::nullopt;
    }

    void awaitChunk(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t waited,
                    std::shared_ptr<std::function<void(bool)>> done) {
        constexpr std::int32_t kGiveUpTicks = 1200;
        if (world_->state().isLoaded(chunkX, chunkZ)) {
            (*done)(true);
            return;
        }
        if (waited >= kGiveUpTicks) {
            (*done)(false);
            return;
        }
        world_->owner().schedule(
            1, [this, chunkX, chunkZ, waited, done] { awaitChunk(chunkX, chunkZ, waited + 1, done); });
    }

    void ensureChunk(std::int32_t chunkX, std::int32_t chunkZ) {
        if (!world_->state().isLoaded(chunkX, chunkZ)) {
            static_cast<void>(world_->state().getOrGenerateChunk(chunkX, chunkZ));
        }
    }

    static constexpr std::size_t kLightBytes = 2048;

    [[nodiscard]] bool hasSkylight() const {
        return config::dimensionFacts(world_->declaration().dimension).hasSkylight;
    }

    static void showChunks(world::LoadedWorld& world,
                           const std::function<std::vector<player::PlayerSession*>()>& players,
                           std::span<const std::int32_t> chunks) {
        if (!players || chunks.empty()) {
            return;
        }
        constexpr std::int32_t kWatchRadius = 16;
        for (const player::PlayerSession* player : players()) {
            if (player->worldName != world.name() || !player->connection().resendChunk) {
                continue;
            }
            const std::int32_t playerChunkX = static_cast<std::int32_t>(std::floor(player->x)) >> 4;
            const std::int32_t playerChunkZ = static_cast<std::int32_t>(std::floor(player->z)) >> 4;
            for (std::size_t at = 0; at + 1 < chunks.size(); at += 2) {
                if (std::abs(chunks[at] - playerChunkX) <= kWatchRadius &&
                    std::abs(chunks[at + 1] - playerChunkZ) <= kWatchRadius) {
                    player->connection().resendChunk(chunks[at], chunks[at + 1]);
                }
            }
        }
    }

    [[nodiscard]] blockUpdates::Target updateTarget() const {
        std::vector<player::PlayerSession*> watchers;
        if (players_) {
            for (player::PlayerSession* player : players_()) {
                if (player->worldName == world_->name()) {
                    watchers.push_back(player);
                }
            }
        }
        return blockUpdates::Target{*world_, hooks_.blocks ? hooks_.blocks() : nullptr, std::move(watchers)};
    }

    void tell(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) const {
        if (!players_) {
            return;
        }
        for (const player::PlayerSession* player : players_()) {
            if (player->worldName == world_->name()) {
                player::sendBlockChangeTo(*player, x, y, z, stateId);
            }
        }
    }

    world::LoadedWorld* world_;
    ModelHooks hooks_;
    std::function<std::vector<player::PlayerSession*>()> players_;
};

} // namespace

std::vector<std::shared_ptr<PluginWorldRef>> LoadedWorlds::all() const {
    std::vector<std::shared_ptr<PluginWorldRef>> refs;
    if (!hooks_.allWorlds) {
        return refs;
    }
    for (world::LoadedWorld* world : hooks_.allWorlds()) {
        refs.push_back(std::make_shared<LoadedWorldRef>(*world, hooks_, players_));
    }
    return refs;
}

std::shared_ptr<PluginWorldRef> LoadedWorlds::byName(std::string_view name) const {
    world::LoadedWorld* world = hooks_.worldFor ? hooks_.worldFor(std::string{name}) : nullptr;
    return world == nullptr ? nullptr : std::make_shared<LoadedWorldRef>(*world, hooks_, players_);
}

std::shared_ptr<PluginWorldRef> LoadedWorlds::defaultWorld() const {
    return hooks_.defaultWorldName ? byName(hooks_.defaultWorldName()) : nullptr;
}

std::shared_ptr<PluginWorldRef> LoadedWorlds::create(std::string_view name, std::string_view dimension,
                                                     std::string_view type, std::optional<std::int64_t> seed) {
    // Named and refused rather than guessed: a typo that quietly made an overworld is a world to delete by hand.
    std::string dimensionName{dimension};
    if (dimensionName.starts_with("minecraft:")) {
        dimensionName = dimensionName.substr(std::string_view{"minecraft:"}.size());
    }
    const std::optional<config::WorldDimension> theDimension = config::dimensionByName(dimensionName);
    const std::optional<config::WorldType> theType = config::worldTypeByName(type);
    if (!theDimension.has_value() || !theType.has_value() || !hooks_.makeWorld) {
        return nullptr;
    }
    world::LoadedWorld* made = hooks_.makeWorld(std::string{name}, *theDimension, *theType, seed);
    return made == nullptr ? nullptr : std::make_shared<LoadedWorldRef>(*made, hooks_, players_);
}

std::int32_t LoadedWorlds::stateId(std::string_view text) const {
    return LoadedWorldRef::parseState(text).value_or(-1);
}

std::string LoadedWorlds::stateText(std::int32_t stateId) const {
    const Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return {};
    }
    std::string text = block->toString();
    const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(stateId);
    char separator = '[';
    for (const auto& [name, value] : properties) {
        text += separator;
        text += name + "=" + value;
        separator = ',';
    }
    return properties.empty() ? text : text + "]";
}

std::vector<std::string> LoadedWorlds::blockTable() const {
    std::vector<std::string> table;
    const reg::Registry<reg::BlockState>& blocks = reg::blocks::registry();
    for (const std::int32_t defaultState : blocks.ids()) {
        const Identifier* name = blocks.nameOf(defaultState);
        const reg::blockStateDefinitions::Definition* definition = reg::blockStateDefinitions::of(*name);
        table.push_back(name->toString());
        table.push_back(std::to_string(definition == nullptr ? defaultState : definition->firstStateId()));
        table.push_back(std::to_string(definition == nullptr ? 1 : definition->stateCount()));
    }
    return table;
}

std::vector<std::string> LoadedWorlds::blockProperties(std::string_view block) const {
    std::vector<std::string> flat;
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(std::string{block});
    const reg::blockStateDefinitions::Definition* definition =
        id.has_value() ? reg::blockStateDefinitions::of(*id) : nullptr;
    if (definition == nullptr) {
        return flat;
    }
    for (const std::string& name : definition->propertyNames()) {
        std::string joined;
        for (const std::string& value : definition->valuesOf(name)) {
            joined += joined.empty() ? value : "|" + value;
        }
        flat.push_back(name);
        flat.push_back(joined);
    }
    return flat;
}

std::int32_t LoadedWorlds::stateFlags(std::int32_t stateId) const {
    std::int32_t bits = 0;
    bits |= reg::blockShapes::blocksMotion(stateId) ? 1 : 0;
    bits |= reg::blockShapes::isSolidRender(stateId) ? 2 : 0;
    bits |= reg::blockShapes::isFullCube(stateId) ? 4 : 0;
    bits |= reg::blockShapes::isReplaceable(stateId) ? 8 : 0;
    bits |= reg::blockFluids::hasFluid(stateId) ? 16 : 0;
    bits |= reg::blockShapes::isEmptyShape(stateId) ? 0 : 32;
    bits |= reg::blockSupport::conductsRedstone(stateId) ? 64 : 0;
    bits |= reg::blockSupport::ticksRandomly(stateId) ? 128 : 0;
    bits |= reg::blocks::isAir(stateId) ? 256 : 0;
    bits |= server::world::light::blockLight::emission(stateId) << 9;
    bits |= server::world::light::skyLight::opacity(stateId) << 13;
    const Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
    bits |= block != nullptr && reg::blockEntityTypes::ofBlock(*block) != nullptr ? (1 << 18) : 0;
    return bits;
}

std::vector<double> LoadedWorlds::blockHardness(std::string_view block) const {
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(std::string{block});
    const reg::BlockHardness* found = id.has_value() ? reg::blocks::hardnessOf(*id) : nullptr;
    if (found == nullptr) {
        return {};
    }
    return {static_cast<double>(found->destroyTime), static_cast<double>(found->explosionResistance),
            found->requiresCorrectTool ? 1.0 : 0.0};
}

std::vector<double> LoadedWorlds::blockInfo(std::int32_t stateId) const {
    if (reg::blockStateDefinitions::blockOf(stateId) == nullptr) {
        return {};
    }
    const std::uint32_t color =
        reg::mapColors::argbOf(reg::mapColors::packed(reg::mapColors::of(stateId), reg::mapColors::Brightness::High));
    return {static_cast<double>(reg::blockMovement::frictionOf(stateId)),
            static_cast<double>(reg::blockMovement::speedFactorOf(stateId)),
            static_cast<double>(reg::blockMovement::jumpFactorOf(stateId)),
            static_cast<double>(static_cast<std::int32_t>(reg::blockSupport::pushReaction(stateId))),
            static_cast<double>(color & 0xFFFFFFU),
            static_cast<double>(reg::fireOdds::igniteOdds(stateId)),
            static_cast<double>(reg::fireOdds::burnOdds(stateId))};
}

std::vector<std::int32_t> LoadedWorlds::collisionBoxes(std::int32_t stateId) const {
    if (reg::blockStateDefinitions::blockOf(stateId) == nullptr) {
        return {};
    }
    const std::span<const std::int32_t> boxes = reg::blockShapes::collisionBoxes(stateId);
    return {boxes.begin(), boxes.end()};
}

std::vector<std::string> LoadedWorlds::blockEntityTypes() const {
    std::vector<std::string> pairs;
    const reg::Registry<reg::BlockState>& blocks = reg::blocks::registry();
    for (const std::int32_t defaultState : blocks.ids()) {
        const Identifier* name = blocks.nameOf(defaultState);
        if (const Identifier* type = reg::blockEntityTypes::ofBlock(*name); type != nullptr) {
            pairs.push_back(name->toString());
            pairs.push_back(type->toString());
        }
    }
    return pairs;
}

std::int32_t LoadedWorlds::dataVersion() const { return dataFix::currentVersion(); }

std::optional<std::vector<std::uint8_t>> LoadedWorlds::fixNbt(std::string_view kind, std::span<const std::uint8_t> nbt,
                                                              std::int32_t fromVersion) const {
    const auto parsed = wire::nbt::nbtIO::read(nbt);
    const wire::nbt::NbtCompound* given = parsed.has_value() ? parsed->tag.get<wire::nbt::NbtCompound>() : nullptr;
    if (given == nullptr) {
        return std::nullopt;
    }
    std::optional<wire::nbt::NbtCompound> fixed = dataFix::fixDocument(kind, *given, fromVersion);
    if (!fixed.has_value()) {
        return std::nullopt;
    }
    return wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(std::move(*fixed)));
}

std::optional<std::string> LoadedWorlds::fixName(std::string_view kind, std::string_view text,
                                                 std::int32_t fromVersion) const {
    return dataFix::fixName(kind, text, fromVersion);
}

std::vector<std::string> LoadedWorlds::biomeIds() const {
    std::vector<std::string> ids;
    for (const auto& [id, name] : reg::biomeIdsTable()) {
        ids.push_back("minecraft:" + std::string{name});
    }
    return ids;
}

std::vector<std::string> LoadedWorlds::tags(std::string_view registry) const {
    return hooks_.tags ? hooks_.tags(registry) : std::vector<std::string>{};
}

std::vector<std::string> LoadedWorlds::itemIds() const {
    std::vector<std::string> ids;
    for (const auto& [id, name] : reg::itemIdsTable()) {
        ids.push_back("minecraft:" + std::string{name});
    }
    return ids;
}

std::vector<std::string> LoadedWorlds::registryIds(std::string_view registry) const {
    const auto tableIds = [](auto table) {
        std::vector<std::string> ids;
        for (const auto& [id, name] : table) {
            ids.push_back("minecraft:" + std::string{name});
        }
        return ids;
    };
    if (registry == "item") {
        return itemIds();
    }
    if (registry == "entity_type") {
        return entityTypeIds();
    }
    if (registry == "worldgen/biome") {
        return biomeIds();
    }
    if (registry == "particle_type") {
        return tableIds(reg::particleTypeIdsTable());
    }
    if (registry == "data_component_type") {
        return tableIds(reg::dataComponentIdsTable());
    }
    if (registry == "map_decoration_type") {
        std::vector<std::string> ids;
        for (std::int32_t id = 0; world::maps::decorationTypes::byId(id) != nullptr; ++id) {
            ids.push_back("minecraft:" + std::string{world::maps::decorationTypes::byId(id)->name});
        }
        return ids;
    }
    if (registry == "block_entity_type") {
        std::vector<std::string> ids;
        for (const std::string_view name : reg::blockEntityTypeNamesTable()) {
            ids.push_back("minecraft:" + std::string{name});
        }
        return ids;
    }
    static const std::vector<wire::protocol::configuration::RegistryDataPacket> synced =
        registry::vanillaConfigurationRegistries::packets();
    for (const wire::protocol::configuration::RegistryDataPacket& packet : synced) {
        if (packet.registryId == "minecraft:" + std::string{registry}) {
            std::vector<std::string> ids;
            for (const wire::protocol::configuration::RegistryEntry& entry : packet.entries) {
                ids.push_back(entry.key);
            }
            return ids;
        }
    }
    return {};
}

std::vector<std::string> LoadedWorlds::entityTypeIds() const {
    std::vector<std::string> ids;
    for (const reg::EntityTypeEntry& entry : reg::entityTypes::all()) {
        ids.push_back(entry.id.toString());
    }
    return ids;
}

server::plugins::PluginHandle RegistryPlaceholders::registerPlaceholder(std::string_view pluginId,
                                                                        std::string_view name, Value value) {
    const std::string identifier = std::string{pluginId} + "_" + std::string{name};
    server::plugins::PluginPlayers* players = players_;
    registry_->registerProvider(
        identifier, [players, value](std::string_view, const wire::common::placeholder::PlaceholderContext& context) {
            const player::PlayerSession* session = context.get(placeholder::playerSlot());
            const std::shared_ptr<server::plugins::PluginPlayerRef> player =
                session == nullptr ? nullptr : players->byName(session->name());
            return wire::common::placeholder::PlaceholderValue::of(value(player.get()));
        });
    wire::common::placeholder::PlaceholderRegistry* registry = registry_;
    return server::plugins::PluginHandle{[registry, identifier] { registry->unregisterProvider(identifier); }};
}

std::string RegistryPlaceholders::fill(std::string_view text, server::plugins::PluginPlayerRef* player) const {
    const player::PlayerSession* session = nullptr;
    if (player != nullptr && sessions_) {
        const std::string name = player->name();
        for (const player::PlayerSession* candidate : sessions_()) {
            if (candidate->name() == name) {
                session = candidate;
                break;
            }
        }
    }
    const wire::common::placeholder::PlaceholderContext context =
        wire::common::placeholder::PlaceholderContext{}.with(placeholder::playerSlot(), session);
    return wire::common::placeholder::substitute(text, *registry_, context);
}

std::optional<std::string> DatabasePluginStorage::playerValue(std::string_view pluginId, std::string_view uuid,
                                                              std::string_view key) {
    storage::PlayerStore* store = store_();
    return store == nullptr || !store->isOpen() ? std::nullopt : store->pluginData().get(pluginId, uuid, key);
}

void DatabasePluginStorage::setPlayerValue(std::string_view pluginId, std::string_view uuid, std::string_view key,
                                           const std::optional<std::string>& value) {
    storage::PlayerStore* store = store_();
    if (store != nullptr && store->isOpen()) {
        store->pluginData().set(pluginId, uuid, key, value);
    }
}

std::map<std::string, std::string> DatabasePluginStorage::playerValues(std::string_view pluginId,
                                                                       std::string_view uuid) {
    storage::PlayerStore* store = store_();
    if (store == nullptr || !store->isOpen()) {
        return {};
    }
    const std::map<std::string, std::string, std::less<>> stored = store->pluginData().all(pluginId, uuid);
    return std::map<std::string, std::string>{stored.begin(), stored.end()};
}

std::vector<std::pair<std::string, std::string>> DatabasePluginStorage::top(std::string_view pluginId,
                                                                            std::string_view key, std::int32_t limit) {
    storage::PlayerStore* store = store_();
    return store == nullptr || !store->isOpen() ? std::vector<std::pair<std::string, std::string>>{}
                                                : store->pluginData().top(pluginId, key, limit);
}

std::map<std::string, std::int64_t> DatabasePluginStorage::namespaces() {
    storage::PlayerStore* store = store_();
    if (store == nullptr || !store->isOpen()) {
        return {};
    }
    const std::map<std::string, std::int64_t, std::less<>> counted = store->pluginData().namespaces();
    return std::map<std::string, std::int64_t>{counted.begin(), counted.end()};
}

std::optional<std::vector<std::uint8_t>> DatabasePluginStorage::holderData(std::string_view kind, std::string_view id) {
    storage::PlayerStore* store = store_();
    return store == nullptr || !store->isOpen() ? std::nullopt : store->pluginData().holder(kind, id);
}

void DatabasePluginStorage::setHolderData(std::string_view kind, std::string_view id,
                                          std::span<const std::uint8_t> data) {
    storage::PlayerStore* store = store_();
    if (store != nullptr && store->isOpen()) {
        store->pluginData().setHolder(kind, id, data);
    }
}

std::int64_t DatabasePluginStorage::purge(std::string_view pluginId) {
    storage::PlayerStore* store = store_();
    if (store == nullptr || !store->isOpen()) {
        return 0;
    }
    const std::map<std::string, std::int64_t, std::less<>> counted = store->pluginData().namespaces();
    const auto found = counted.find(pluginId);
    if (found == counted.end()) {
        return 0;
    }
    store->pluginData().purge(pluginId);
    return found->second;
}

} // namespace basalt::server::runtime::plugins
