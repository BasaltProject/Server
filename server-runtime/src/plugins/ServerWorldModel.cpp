#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
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

    bool setBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view block) override {
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(std::string{block});
        const std::optional<std::int32_t> stateId = id.has_value() ? reg::blocks::defaultStateIdOf(*id) : std::nullopt;
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
