#include "DataFix.hpp"
#include "ServerWorldRefs.hpp"
#include "WorldBlockUpdates.hpp"
#include "WorldItemUse.hpp"

#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/MapColors.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/VanillaTables.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/DroppedItemOps.hpp>
#include <basalt/server/runtime/plugins/EntityOps.hpp>
#include <basalt/server/runtime/plugins/PlayerEntityOps.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/plugins/ServerWorldEnv.hpp>
#include <basalt/server/runtime/registry/VanillaConfigurationRegistries.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/server/runtime/world/maps/MapDecorationTypes.hpp>
#include <basalt/server/runtime/world/poi/PoiTypes.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/light/Light.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/decoration/end/EndSpikes.hpp>
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

namespace basalt::server::runtime::plugins::worldModel {
namespace reg = server::registry;
using wire::common::Identifier;

LoadedWorldRef::LoadedWorldRef(world::LoadedWorld& world, std::shared_ptr<const ModelHooks> hooks,
                               std::function<std::vector<player::PlayerSession*>()> players)
    : world_(&world), shared_(std::move(hooks)), hooks_(*shared_), players_(std::move(players)),
      env_(world, shared_, players_) {}

server::plugins::PluginWorldEnv* LoadedWorldRef::env() { return &env_; }

std::string LoadedWorldRef::name() const { return world_->name(); }

std::int64_t LoadedWorldRef::time() const { return world_->clock().totalTicks(); }

bool LoadedWorldRef::raining() const { return world_->weather().raining; }

bool LoadedWorldRef::thundering() const { return world_->weather().thundering; }

std::string LoadedWorldRef::difficulty() const { return world_->difficulty(); }

std::optional<std::string> LoadedWorldRef::gameRule(std::string_view name) const {
    const world::GameRule* rule = world::gameRules::byId(name);
    return rule == nullptr ? std::nullopt : std::optional{world_->gameRules().serialized(*rule)};
}

bool LoadedWorldRef::setGameRule(std::string_view name, std::string_view value) {
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

std::vector<std::string> LoadedWorldRef::gameRules() const {
    std::vector<std::string> ids;
    for (const world::GameRule& rule : world::gameRules::all()) {
        ids.emplace_back(rule.id);
    }
    return ids;
}

std::optional<std::string> LoadedWorldRef::block(std::int32_t x, std::int32_t y, std::int32_t z) const {
    const std::optional<std::int32_t> stateId = world_->state().blockState(x, y, z);
    const Identifier* block = stateId.has_value() ? reg::blockStateDefinitions::blockOf(*stateId) : nullptr;
    return block == nullptr ? std::nullopt : std::optional{block->toString()};
}

std::optional<std::string> LoadedWorldRef::blockState(std::int32_t x, std::int32_t y, std::int32_t z) const {
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

std::optional<std::int32_t> LoadedWorldRef::parseState(std::string_view text) {
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

bool LoadedWorldRef::setBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view block) {
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

std::string LoadedWorldRef::dimension() const {
    return "minecraft:" + std::string{config::dimensionName(world_->declaration().dimension)};
}

std::int64_t LoadedWorldRef::seed() const { return world_->metadata().seed; }

std::int32_t LoadedWorldRef::minHeight() const { return chunkHeight().minY(); }

std::int32_t LoadedWorldRef::height() const { return chunkHeight().height(); }

std::array<std::int32_t, 3> LoadedWorldRef::spawn() const {
    const std::array<double, 3>& at = world_->spawn();
    return {static_cast<std::int32_t>(std::floor(at[0])), static_cast<std::int32_t>(std::floor(at[1])),
            static_cast<std::int32_t>(std::floor(at[2]))};
}

void LoadedWorldRef::setSpawn(std::int32_t x, std::int32_t y, std::int32_t z) { world_->setSpawn(x, y, z); }

std::int64_t LoadedWorldRef::dayTime() const { return world_->clock().totalTicks(); }

void LoadedWorldRef::setDayTime(std::int64_t ticks) {
    world_->clock().setTotalTicks(ticks);
    if (hooks_.environment) {
        hooks_.environment().syncTime(*world_);
    }
}

void LoadedWorldRef::setWeather(bool raining, bool thundering) {
    world_->weather().raining = raining;
    world_->weather().thundering = thundering;
    if (hooks_.environment) {
        hooks_.environment().syncWeather(*world_);
    }
}

bool LoadedWorldRef::setDifficulty(std::string_view name) {
    if (name != "peaceful" && name != "easy" && name != "normal" && name != "hard") {
        return false;
    }
    world_->setDifficulty(std::string{name});
    return true;
}

std::int32_t LoadedWorldRef::stateAt(std::int32_t x, std::int32_t y, std::int32_t z, bool load) {
    if (load) {
        ensureChunk(x >> 4, z >> 4);
    }
    return world_->state().blockState(x, y, z).value_or(-1);
}

bool LoadedWorldRef::setStateAt(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, bool notify) {
    ensureChunk(x >> 4, z >> 4);
    if (!world_->state().setBlockState(x, y, z, stateId)) {
        return false;
    }
    if (notify) {
        tell(x, y, z, stateId);
    }
    return true;
}

std::int32_t LoadedWorldRef::readStates(std::span<const std::int32_t> positions, std::span<std::int32_t> out,
                                        bool load) {
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

std::int32_t LoadedWorldRef::writeStates(std::span<const std::int32_t> positions, std::span<const std::int32_t> states,
                                         bool notify) {
    const std::size_t count = std::min(positions.size() / 3, states.size());
    std::int32_t changed = 0;
    for (std::size_t index = 0; index < count; ++index) {
        changed +=
            setStateAt(positions[index * 3], positions[index * 3 + 1], positions[index * 3 + 2], states[index], notify)
                ? 1
                : 0;
    }
    return changed;
}

bool LoadedWorldRef::readSection(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ, bool load,
                                 std::span<std::int32_t> out) {
    if (load) {
        ensureChunk(chunkX, chunkZ);
    }
    return world_->state().copySection(chunkX, sectionY, chunkZ, out);
}

bool LoadedWorldRef::readSections(std::int32_t chunkX, std::int32_t chunkZ, bool load, std::span<std::int32_t> out) {
    if (load) {
        ensureChunk(chunkX, chunkZ);
    }
    return world_->state().copySections(chunkX, chunkZ, out);
}

std::int32_t LoadedWorldRef::chunkStatus(std::int32_t chunkX, std::int32_t chunkZ) const {
    constexpr std::int32_t kSimulationRadius = 10;
    if (!world_->state().isLoaded(chunkX, chunkZ)) {
        return world_->generationQueue().pendingKeys().contains(world::ChunkGenerationQueue::pack(chunkX, chunkZ)) ? 1
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

void LoadedWorldRef::requestChunk(std::int32_t chunkX, std::int32_t chunkZ, bool generate, std::int32_t priority,
                                  std::function<void(bool)> done) {
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
    world::LoadedWorld* held = world_;
    awaitChunk(held, chunkX, chunkZ, 0,
               std::make_shared<std::function<void(bool)>>([held, token, done = std::move(done)](bool loaded) {
                   done(loaded);
                   held->releaseChunks(token);
               }));
}

bool LoadedWorldRef::holdChunks(std::string_view token, std::span<const std::int32_t> chunks) {
    std::vector<std::int64_t> keys;
    for (std::size_t at = 0; at + 1 < chunks.size(); at += 2) {
        keys.push_back(world::ChunkGenerationQueue::pack(chunks[at], chunks[at + 1]));
    }
    world_->holdChunks(std::string{token}, keys);
    return true;
}

void LoadedWorldRef::releaseChunks(std::string_view token) { world_->releaseChunks(std::string{token}); }

std::int32_t LoadedWorldRef::settleBlocks(std::span<const std::int32_t> positions,
                                          std::span<const std::int32_t> previous, std::uint32_t effects) {
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

std::int32_t LoadedWorldRef::neighborChanged(std::span<const std::int32_t> positions,
                                             std::span<const std::int32_t> sources) {
    return blockUpdates::neighbourChanged(updateTarget(), positions, sources);
}
} // namespace basalt::server::runtime::plugins::worldModel
