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

class MapSystem {
public:
    struct Options {
        std::function<world::LoadedWorld*(std::string_view name)> worldNamed;
        const world::realItemTags::TagMembers* itemTags = nullptr;
        std::function<std::int64_t()> nextSeed;
        std::function<std::optional<std::string>(std::string_view name)> readJar;
    };

    MapSystem(player::PlayerRegistry& players, world::maps::MapStorage& storage, Options options);

    [[nodiscard]] static bool isMap(std::string_view item);

    [[nodiscard]] static std::optional<std::int32_t> mapIdOf(const wire::nbt::NbtCompound& components);

    void tickPlayers(world::LoadedWorld& world);

    void tickFrames(world::LoadedWorld& world);

    bool useEmptyMap(const player::BreakContext& context, player::PlayerSession& player, world::LoadedWorld& world,
                     std::int32_t slot);

    /// parity: `MapItem.useOn` against a banner.
    [[nodiscard]] std::optional<bool> useOnBanner(player::PlayerSession& player, world::LoadedWorld& world,
                                                  std::int32_t slot, std::int32_t x, std::int32_t y, std::int32_t z);

    void postProcessCrafted(wire::nbt::NbtCompound& components, const world::LoadedWorld& world);

    bool explore(world::LoadedWorld& world, wire::nbt::NbtCompound& components,
                 const server::world::loot::ExplorationMapRequest& request, const std::array<double, 3>& origin);

    void frameLostMap(const wire::nbt::NbtCompound& components, const entity::ServerEntity& frame);

    [[nodiscard]] bool tooManyMarkers(const wire::nbt::NbtCompound& components);

    void sendWhole(const player::PlayerSession& player, std::int32_t mapId);

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

    void makeExplorerMapLocked(wire::nbt::NbtCompound& components, const world::LoadedWorld& world,
                               std::int32_t targetX, std::int32_t targetZ, std::int8_t zoom,
                               std::string_view decoration,
                               const std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z)>& watery,
                               std::int32_t seaLevel);

    void sweepFrames(world::LoadedWorld& world, const std::vector<entity::ServerEntity*>& frames);

    [[nodiscard]] std::vector<wire::common::Identifier> structuresIn(std::string_view tag,
                                                                     std::int32_t depth = 0) const;

    player::PlayerRegistry* players_;
    world::maps::MapStorage* storage_;
    Options options_;
    std::optional<std::set<std::string>> wateryBiomes_;
};

void sendMapUpdate(const player::PlayerSession& player, const wire::protocol::play::MapItemDataPacket& update);

} // namespace basalt::server::runtime
