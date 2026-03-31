#pragma once

#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/maps/MapData.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/server/world/loot/LootContext.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::player {
struct BreakContext;
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;

namespace maps {
class MapStorage;
}
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {

/// Maps as items: making one, drawing it as it is carried, the markers of whoever carries it or
/// hangs it, and sending what changed.
///
/// parity: `EmptyMapItem`, `MapItem`, `MapItemSavedData.tickCarriedBy` and the map halves of
/// `ServerPlayer.doTick` and `ServerEntity.sendChanges`.
class MapSystem {
public:
    struct Options {
        std::function<world::LoadedWorld*(std::string_view name)> worldNamed;
        const world::realItemTags::TagMembers* itemTags = nullptr;
        std::function<std::int64_t()> nextSeed;
        /// One file of the operator's jar, for the structure and biome tags an explorer map reads.
        std::function<std::optional<std::string>(std::string_view name)> readJar;
    };

    MapSystem(player::PlayerRegistry& players, world::maps::MapStorage& storage, Options options);

    /// parity: `item instanceof MapItem`: the filled map and every explorer map.
    [[nodiscard]] static bool isMap(std::string_view item);

    /// parity: `DataComponents.MAP_ID`.
    [[nodiscard]] static std::optional<std::int32_t> mapIdOf(const wire::nbt::NbtCompound& components);

    /// The inventory half of every player's turn in this world, then the updates they are owed.
    void tickPlayers(world::LoadedWorld& world);

    /// parity: `ServerEntity.sendChanges` for a frame with a map, every tenth tick.
    void tickFrames(world::LoadedWorld& world);

    /// parity: `EmptyMapItem.use`. False when the slot holds no empty map.
    bool useEmptyMap(const player::BreakContext& context, player::PlayerSession& player, world::LoadedWorld& world,
                     std::int32_t slot);

    /// parity: `MapItem.useOn` against a banner. Nothing when the click was not a map on a banner.
    [[nodiscard]] std::optional<bool> useOnBanner(player::PlayerSession& player, world::LoadedWorld& world,
                                                  std::int32_t slot, std::int32_t x, std::int32_t y, std::int32_t z);

    /// parity: `MapItem.onCraftedPostProcess`: a `map_post_processing` becomes a new map.
    void postProcessCrafted(wire::nbt::NbtCompound& components, const world::LoadedWorld& world);

    /// parity: `ExplorationMapFunction.run`: the nearest wanted structure from `origin`, and the
    /// map to it. False when there is none, which leaves the stack as it was.
    bool explore(world::LoadedWorld& world, wire::nbt::NbtCompound& components,
                 const server::world::loot::ExplorationMapRequest& request, const std::array<double, 3>& origin);

    /// The whole of a map, colours and markers, for a client that asks for one it was never sent.
    void sendWhole(const player::PlayerSession& player, std::int32_t mapId);

    /// Scale and lock of a map, for the cartography table. Nothing for an unknown map.
    struct Summary {
        std::int8_t scale = 0;
        bool locked = false;
    };

    [[nodiscard]] std::optional<Summary> summaryOf(std::int32_t mapId);

    void save();

private:
    struct FrameView {
        world::maps::MapFrame frame;
    };

    void tickCarriedBy(world::maps::MapData& data, player::PlayerSession& ticking,
                       const storage::StoredItemStack& stack, const FrameView* frame, std::int64_t gameTime);

    [[nodiscard]] bool carries(const player::PlayerSession& player, std::string_view item, std::int32_t mapId) const;

    [[nodiscard]] bool hidden(const player::PlayerSession& player) const;

    void update(world::LoadedWorld& world, world::maps::MapData& data, player::PlayerSession& player);

    /// parity: `MapItem.applyNewSavedData`, `renderBiomePreviewMap` and `addTargetDecoration`. The
    /// storage lock is already held.
    void makeExplorerMapLocked(wire::nbt::NbtCompound& components, const world::LoadedWorld& world,
                               std::int32_t targetX, std::int32_t targetZ, std::int8_t zoom,
                               std::string_view decoration,
                               const std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z)>& watery,
                               std::int32_t seaLevel);

    void sweepFrames(world::LoadedWorld& world, const std::vector<entity::ServerEntity*>& frames);

    /// The structures in a `worldgen/structure` tag, followed through the tags it names.
    [[nodiscard]] std::vector<wire::common::Identifier> structuresIn(std::string_view tag,
                                                                     std::int32_t depth = 0) const;

    player::PlayerRegistry* players_;
    world::maps::MapStorage* storage_;
    Options options_;
    /// parity: `BiomeTags.WATER_ON_MAP_OUTLINES`, read once.
    std::optional<std::set<std::string>> wateryBiomes_;
};

/// Sends one map update to a player, whichever edition they are on.
void sendMapUpdate(const player::PlayerSession& player, const wire::protocol::play::MapItemDataPacket& update);

} // namespace basalt::server::runtime
