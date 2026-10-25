#pragma once

#include <basalt/server/plugins/PluginEntityOps.hpp>
#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/plugins/PluginWorldRef.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

class PluginWorlds {
public:
    PluginWorlds() = default;
    PluginWorlds(const PluginWorlds&) = delete;
    PluginWorlds& operator=(const PluginWorlds&) = delete;
    PluginWorlds(PluginWorlds&&) = delete;
    PluginWorlds& operator=(PluginWorlds&&) = delete;
    virtual ~PluginWorlds() = default;

    [[nodiscard]] virtual std::vector<std::shared_ptr<PluginWorldRef>> all() const = 0;

    [[nodiscard]] virtual std::shared_ptr<PluginWorldRef> byName(std::string_view name) const = 0;

    [[nodiscard]] virtual std::shared_ptr<PluginWorldRef> defaultWorld() const = 0;

    virtual std::shared_ptr<PluginWorldRef> create(std::string_view name, std::string_view dimension,
                                                   std::string_view type, std::optional<std::int64_t> seed) = 0;

    /// Takes a world that holds no player out of play, saving it first when asked; false for the default world.
    virtual bool unload(std::string_view name, bool save) {
        static_cast<void>(name);
        static_cast<void>(save);
        return false;
    }

    [[nodiscard]] virtual std::int32_t stateId(std::string_view text) const {
        static_cast<void>(text);
        return -1;
    }

    /// The reverse of `stateId`; empty for an id that is not a state.
    [[nodiscard]] virtual std::string stateText(std::int32_t stateId) const {
        static_cast<void>(stateId);
        return {};
    }

    /// Every block in registry order as three entries each: its id, its first state id and its state count.
    [[nodiscard]] virtual std::vector<std::string> blockTable() const { return {}; }

    /// The properties of a block as name, then its values joined with `|`, for each in state-id stride order.
    [[nodiscard]] virtual std::vector<std::string> blockProperties(std::string_view block) const {
        static_cast<void>(block);
        return {};
    }

    /// Facts about one state as bits (motion, occlusion, full cube, fluid, ...) plus light emission and opacity.
    [[nodiscard]] virtual std::int32_t stateFlags(std::int32_t stateId) const {
        static_cast<void>(stateId);
        return 0;
    }

    /// Destroy time, explosion resistance and whether the right tool is needed (1 or 0), or empty for an unknown block.
    [[nodiscard]] virtual std::vector<double> blockHardness(std::string_view block) const {
        static_cast<void>(block);
        return {};
    }

    /// The collision boxes of a block state, six numbers each (min then max corner) in 32nds of a block.
    [[nodiscard]] virtual std::vector<std::int32_t> collisionBoxes(std::int32_t stateId) const {
        static_cast<void>(stateId);
        return {};
    }

    /// Physical facts about one state: friction, speed, jump, push reaction, map colour 0xRRGGBB, fire odds.
    [[nodiscard]] virtual std::vector<double> blockInfo(std::int32_t stateId) const {
        static_cast<void>(stateId);
        return {};
    }

    /// The point of interest type a block state is (`armorer`, `home`...), or empty.
    [[nodiscard]] virtual std::string poiTypeOf(std::int32_t stateId) const {
        static_cast<void>(stateId);
        return {};
    }

    [[nodiscard]] virtual std::int32_t poiTickets(std::string_view type) const {
        static_cast<void>(type);
        return 0;
    }

    /// A fresh entity id, the next one the server would give to anything it spawns.
    [[nodiscard]] virtual std::int32_t nextEntityId() { return -1; }

    /// The note block instrument that sounds over a state, by its serialized name (`harp`, `basedrum`...).
    [[nodiscard]] virtual std::string blockInstrument(std::int32_t stateId) const {
        static_cast<void>(stateId);
        return {};
    }

    /// The sound type of one state: volume, pitch, then the break, step, place, hit and fall sound ids.
    [[nodiscard]] virtual std::vector<std::string> blockSounds(std::int32_t stateId) const {
        static_cast<void>(stateId);
        return {};
    }

    /// Whether a tool (an item id, empty for a hand) is right for the drops of a state, then its mining speed.
    [[nodiscard]] virtual std::vector<double> toolFacts(std::string_view item, std::int32_t stateId) const {
        static_cast<void>(item);
        static_cast<void>(stateId);
        return {};
    }

    /// The state turned `rotation` quarter turns clockwise (0 to 3) after a mirror (0 none, 1 left-right, 2 back).
    [[nodiscard]] virtual std::int32_t transformState(std::int32_t stateId, std::int32_t rotation,
                                                      std::int32_t mirror) const {
        static_cast<void>(rotation);
        static_cast<void>(mirror);
        return stateId;
    }

    /// Every game rule as triples: its id, `boolean` or `int`, and the value a world starts with.
    [[nodiscard]] virtual std::vector<std::string> gameRuleSpecs() const { return {}; }

    /// The strength (1 to 15) a sculk sensor gives a game event; zero for one it does not listen to.
    [[nodiscard]] virtual std::int32_t gameEventFrequency(std::string_view event) const {
        static_cast<void>(event);
        return 0;
    }

    /// Which blocks own a block entity, as pairs: the block id, then the id of its block entity type.
    [[nodiscard]] virtual std::vector<std::string> blockEntityTypes() const { return {}; }

    [[nodiscard]] virtual std::int32_t dataVersion() const { return 0; }

    /// Upgrades a named NBT document of `kind` (chunk, block_entity, entity, item) from `fromVersion`; else nothing.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>>
    fixNbt(std::string_view kind, std::span<const std::uint8_t> nbt, std::int32_t fromVersion) const {
        static_cast<void>(kind);
        static_cast<void>(nbt);
        static_cast<void>(fromVersion);
        return std::nullopt;
    }

    /// Upgrades a `block_state` (`minecraft:oak_stairs[facing=east]`), `item` or `biome` id written at `fromVersion`.
    [[nodiscard]] virtual std::optional<std::string> fixName(std::string_view kind, std::string_view text,
                                                             std::int32_t fromVersion) const {
        static_cast<void>(kind);
        static_cast<void>(text);
        static_cast<void>(fromVersion);
        return std::nullopt;
    }

    /// The biome ids, in registry order.
    [[nodiscard]] virtual std::vector<std::string> biomeIds() const { return {}; }

    /// The tags of `blocks`, `items`, `fluids` or `entity_types` as pairs: the tag id, then its members joined by `|`.
    [[nodiscard]] virtual std::vector<std::string> tags(std::string_view registry) const {
        static_cast<void>(registry);
        return {};
    }

    /// The item ids, in registry order.
    [[nodiscard]] virtual std::vector<std::string> itemIds() const { return {}; }

    /// The ids of a registry by name (`damage_type`, `worldgen/biome`...), empty when the server has none.
    [[nodiscard]] virtual std::vector<std::string> registryIds(std::string_view registry) const {
        static_cast<void>(registry);
        return {};
    }

    /// The data of one entry of a client-synced registry (`damage_type`, `instrument`...) as an unnamed NBT compound.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> registryData(std::string_view registry,
                                                                                std::string_view id) const {
        static_cast<void>(registry);
        static_cast<void>(id);
        return std::nullopt;
    }

    /// The attributes a new entity of the type starts with, as `attribute=value`.
    [[nodiscard]] virtual std::vector<std::string> entityDefaultAttributes(std::string_view type) const {
        static_cast<void>(type);
        return {};
    }

    [[nodiscard]] virtual std::vector<std::string> entityTypeIds() const { return {}; }
};

class PluginPlaceholders {
public:
    using Value = std::function<std::string(PluginPlayerRef*)>;

    PluginPlaceholders() = default;
    PluginPlaceholders(const PluginPlaceholders&) = delete;
    PluginPlaceholders& operator=(const PluginPlaceholders&) = delete;
    PluginPlaceholders(PluginPlaceholders&&) = delete;
    PluginPlaceholders& operator=(PluginPlaceholders&&) = delete;
    virtual ~PluginPlaceholders() = default;

    virtual PluginHandle registerPlaceholder(std::string_view pluginId, std::string_view name, Value value) = 0;

    [[nodiscard]] virtual std::string fill(std::string_view text, PluginPlayerRef* player) const = 0;
};
} // namespace basalt::server::plugins
