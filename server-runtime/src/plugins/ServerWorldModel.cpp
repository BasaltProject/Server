#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/VanillaTables.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/light/Light.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/common/placeholder/PlaceholderText.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::plugins {
namespace {

namespace reg = server::registry;

using server::plugins::PluginEntityRef;
using server::plugins::PluginHologramRef;
using server::plugins::PluginWorldRef;
using wire::common::Identifier;

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

    std::int32_t readStates(std::span<const std::int32_t> positions, std::span<std::int32_t> out,
                            bool load) override {
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
        const server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
        const std::int32_t index = sectionY - chunkHeight().minSection();
        if (chunk == nullptr || index < 0 || index >= chunkHeight().sectionCount() ||
            out.size() < server::world::ChunkSection::kBlocks) {
            return false;
        }
        chunk->section(static_cast<std::size_t>(index)).copyBlockStateIdsInto(out);
        return true;
    }

    bool writeSection(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ,
                      std::span<const std::int32_t> states, bool notify) override {
        const std::int32_t index = sectionY - chunkHeight().minSection();
        if (index < 0 || index >= chunkHeight().sectionCount() || states.size() < server::world::ChunkSection::kBlocks) {
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
        if (chunk == nullptr) {
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

    void ensureChunk(std::int32_t chunkX, std::int32_t chunkZ) {
        if (!world_->state().isLoaded(chunkX, chunkZ)) {
            static_cast<void>(world_->state().getOrGenerateChunk(chunkX, chunkZ));
        }
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
