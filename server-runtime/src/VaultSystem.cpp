#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/VaultSystem.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>

#include <algorithm>
#include <cmath>
#include <expected>

namespace basalt::server::runtime {
namespace {

using player::PlayerSession;
using wire::common::Identifier;
using wire::common::Uuid;
namespace vaults = world::vaults;
using server::world::loot::ItemStack;

constexpr const char* kStateProperty = "vault_state";
constexpr std::string_view kVaultBlock = "vault";
/// parity: `VaultBlockEntity.Server.UNLOCKING_DELAY_TICKS`.
constexpr std::int64_t kUnlockingDelay = 14;
/// parity: `VaultState.UPDATE_CONNECTED_PLAYERS_TICK_RATE`, and the three ejection delays that equal it.
constexpr std::int64_t kRecheckTicks = 20;
/// parity: `VaultBlockEntity.Server.DISPLAY_CYCLE_TICK_RATE`.
constexpr std::int64_t kDisplayCycleTicks = 20;
/// parity: `VaultBlockEntity.Server.INSERT_FAIL_SOUND_BUFFER_TICKS`.
constexpr std::int64_t kFailSoundBuffer = 15;
/// parity: `ServerLevel.levelEvent`, heard by the players within sixty-four blocks.
constexpr double kLevelEventRange = 64.0;
constexpr double kUpdateRange = 160.0;
constexpr std::int32_t kActivateEvent = 3015;
constexpr std::int32_t kDeactivateEvent = 3016;
constexpr std::int32_t kEjectEvent = 3017;
constexpr double kEjectHeight = 1.2;
constexpr std::int32_t kEjectAccuracy = 2;

[[nodiscard]] std::string propertyOf(const std::int32_t stateId, const char* name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] bool canEjectReward(const vaults::Config& config, const vaults::State state) {
    return config.keyItem.has_value() && state != vaults::State::Inactive;
}

[[nodiscard]] server::world::Chunk* chunkOf(world::LoadedWorld& world, const std::array<std::int32_t, 3>& pos) {
    return world.state().chunkIfLoaded(pos[0] >> 4, pos[2] >> 4);
}

} // namespace

struct VaultSystem::Turn {
    world::LoadedWorld& world;
    Pos pos;
    std::int32_t stateId = 0;
    vaults::Config config;
    vaults::ServerData server;
    vaults::SharedData shared;
    random::RandomSource random;
    std::int64_t gameTime = 0;

    [[nodiscard]] bool ominous() const { return propertyOf(stateId, "ominous") == "true"; }
};

VaultSystem::VaultSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, Hooks hooks,
                         Options options)
    : players_(&players), broadcaster_(&broadcaster), hooks_(std::move(hooks)), options_(std::move(options)) {}

bool VaultSystem::isActive(world::LoadedWorld& world, const Pos& pos) const {
    const std::optional<std::int32_t> stateId = world.state().blockState(pos[0], pos[1], pos[2]);
    const Identifier* block = stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    const server::world::Chunk* chunk = chunkOf(world, pos);
    return block != nullptr && block->path() == kVaultBlock && chunk != nullptr &&
           chunk->blockEntityAt(pos[0] & 15, pos[1], pos[2] & 15) != nullptr &&
           propertyOf(*stateId, kStateProperty) == vaults::nameOf(vaults::State::Active);
}

bool VaultSystem::tick(world::LoadedWorld& world, const Pos& pos) {
    const std::optional<std::int32_t> stateId = world.state().blockState(pos[0], pos[1], pos[2]);
    const Identifier* block = stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    server::world::Chunk* chunk = chunkOf(world, pos);
    const server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(pos[0] & 15, pos[1], pos[2] & 15);
    const std::optional<vaults::State> current =
        stateId.has_value() ? vaults::stateByName(propertyOf(*stateId, kStateProperty)) : std::nullopt;
    if (block == nullptr || block->path() != kVaultBlock || entity == nullptr || !current.has_value()) {
        return false;
    }
    const std::int64_t gameTime = world.gameTime();
    vaults::ServerData server = vaults::ServerData::read(entity->data());
    const bool cycle = gameTime % kDisplayCycleTicks == 0 && *current == vaults::State::Active;
    if (!cycle && gameTime < server.stateUpdatingResumesAt) {
        return false;
    }
    Turn turn{.world = world,
              .pos = pos,
              .stateId = *stateId,
              .config = vaults::Config::read(entity->data()),
              .server = std::move(server),
              .shared = vaults::SharedData::read(entity->data()),
              .random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0),
              .gameTime = gameTime};
    const vaults::ServerData serverBefore = turn.server;
    const vaults::SharedData sharedBefore = turn.shared;
    if (cycle) {
        cycleDisplayItem(turn, *current);
    }
    if (gameTime >= turn.server.stateUpdatingResumesAt) {
        const vaults::State next = step(turn, *current);
        if (next != *current) {
            changeState(turn, next, nullptr);
        }
    }
    return commit(turn, serverBefore, sharedBefore);
}

std::int32_t VaultSystem::tryInsertKey(world::LoadedWorld& world, PlayerSession& player, const Pos& pos,
                                       const ItemStack& held) {
    const std::optional<std::int32_t> stateId = world.state().blockState(pos[0], pos[1], pos[2]);
    server::world::Chunk* chunk = chunkOf(world, pos);
    const server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(pos[0] & 15, pos[1], pos[2] & 15);
    const std::optional<vaults::State> state =
        stateId.has_value() ? vaults::stateByName(propertyOf(*stateId, kStateProperty)) : std::nullopt;
    if (entity == nullptr || !state.has_value()) {
        return 0;
    }
    Turn turn{.world = world,
              .pos = pos,
              .stateId = *stateId,
              .config = vaults::Config::read(entity->data()),
              .server = vaults::ServerData::read(entity->data()),
              .shared = vaults::SharedData::read(entity->data()),
              .random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0),
              .gameTime = world.gameTime()};
    if (!canEjectReward(turn.config, *state)) {
        return 0;
    }
    const ItemStack& key = *turn.config.keyItem;
    if (held.item != key.item || !(held.components == key.components) || held.count < key.count) {
        failSound(turn, "block.vault.insert_item_fail");
        return 0;
    }
    if (turn.server.hasRewarded(player.uuid())) {
        failSound(turn, "block.vault.reject_rewarded_player");
        return 0;
    }
    Loot loot = roll(turn, turn.config.lootTable, &player, &held);
    if (loot.empty()) {
        return 0;
    }
    const vaults::ServerData serverBefore = turn.server;
    const vaults::SharedData sharedBefore = turn.shared;
    player::statAwards::used(player, held.item.toString());
    // parity: Paper spends the key before `BlockDispenseLootEvent` can cancel the dispensing.
    if (hooks_.dispenseLoot && !hooks_.dispenseLoot(player, world, pos, turn.config.lootTable, loot)) {
        return key.count;
    }
    turn.server.itemsToEject = std::move(loot);
    turn.server.totalEjectionsNeeded = static_cast<std::int32_t>(turn.server.itemsToEject.size());
    turn.shared.displayItem =
        turn.server.itemsToEject.empty() ? std::nullopt : std::optional{turn.server.itemsToEject.back()};
    turn.server.stateUpdatingResumesAt = turn.gameTime + kUnlockingDelay;
    changeState(turn, vaults::State::Unlocking, &player);
    turn.server.addRewarded(player.uuid());
    updateConnected(turn, turn.config.deactivationRange);
    if (commit(turn, serverBefore, sharedBefore)) {
        if (server::world::Chunk* again = chunkOf(world, pos); again != nullptr) {
            again->markDirty();
        }
    }
    return key.count;
}

vaults::State VaultSystem::step(Turn& turn, const vaults::State current) {
    vaults::ServerData& server = turn.server;
    switch (current) {
    case vaults::State::Inactive:
        return settle(turn, turn.config.activationRange);
    case vaults::State::Active:
        return settle(turn, turn.config.deactivationRange);
    case vaults::State::Unlocking:
        server.stateUpdatingResumesAt = turn.gameTime + kRecheckTicks;
        return vaults::State::Ejecting;
    case vaults::State::Ejecting:
        break;
    }
    if (server.itemsToEject.empty()) {
        server.totalEjectionsNeeded = 0;
        return settle(turn, turn.config.deactivationRange);
    }
    const float progress = server.ejectionProgress();
    const ItemStack next = std::move(server.itemsToEject.back());
    server.itemsToEject.pop_back();
    eject(turn, next, progress);
    turn.shared.displayItem = server.itemsToEject.empty() ? std::nullopt : std::optional{server.itemsToEject.back()};
    server.stateUpdatingResumesAt = turn.gameTime + kRecheckTicks;
    return vaults::State::Ejecting;
}

vaults::State VaultSystem::settle(Turn& turn, const double range) {
    updateConnected(turn, range);
    turn.server.stateUpdatingResumesAt = turn.gameTime + kRecheckTicks;
    return turn.shared.connectedPlayers.empty() ? vaults::State::Inactive : vaults::State::Active;
}

void VaultSystem::updateConnected(Turn& turn, const double range) {
    std::vector<Uuid> nearby;
    for (const PlayerSession* player : players_->snapshot()) {
        if (player->worldName != turn.world.name() || player->gamemode == wire::protocol::play::Gamemode::Spectator ||
            turn.server.hasRewarded(player->uuid())) {
            continue;
        }
        // parity: `PlayerDetector.INCLUDING_CREATIVE_PLAYERS` over `BlockPos.closerThan`.
        const double dx = std::floor(player->x) - turn.pos[0];
        const double dy = std::floor(player->y) - turn.pos[1];
        const double dz = std::floor(player->z) - turn.pos[2];
        if ((dx * dx) + (dy * dy) + (dz * dz) < range * range) {
            nearby.push_back(player->uuid());
        }
    }
    if (!turn.shared.sameConnected(nearby)) {
        turn.shared.connectedPlayers = std::move(nearby);
    }
}

void VaultSystem::cycleDisplayItem(Turn& turn, const vaults::State state) {
    if (!canEjectReward(turn.config, state)) {
        turn.shared.displayItem.reset();
        return;
    }
    const Loot results = roll(turn, turn.config.displayTable.value_or(turn.config.lootTable), nullptr, nullptr);
    std::optional<ItemStack> display;
    if (!results.empty()) {
        display = results[static_cast<std::size_t>(turn.random.nextInt(static_cast<std::int32_t>(results.size())))];
    }
    if (hooks_.displayItem && !hooks_.displayItem(turn.world, turn.pos, display)) {
        return;
    }
    turn.shared.displayItem = std::move(display);
}

void VaultSystem::changeState(Turn& turn, const vaults::State to, const PlayerSession* who) {
    const std::optional<vaults::State> from = vaults::stateByName(propertyOf(turn.stateId, kStateProperty));
    const std::optional<std::int32_t> now = turn.world.state().blockState(turn.pos[0], turn.pos[1], turn.pos[2]);
    const Identifier* block = now.has_value() ? registry::blockStateDefinitions::blockOf(*now) : nullptr;
    if (!from.has_value() || block == nullptr) {
        return;
    }
    if (who == nullptr && to == vaults::State::Active && !turn.shared.connectedPlayers.empty()) {
        for (const PlayerSession* candidate : players_->snapshot()) {
            if (candidate->uuid() == turn.shared.connectedPlayers.front()) {
                who = candidate;
                break;
            }
        }
    }
    if (hooks_.changeState && !hooks_.changeState(turn.world, turn.pos, who, *from, to)) {
        return;
    }
    const std::optional<std::int32_t> changed =
        registry::blockStateDefinitions::stateIdOf(*block, {{kStateProperty, std::string{vaults::nameOf(to)}}}, *now);
    if (!changed.has_value() || *changed == *now ||
        !turn.world.state().setBlockState(turn.pos[0], turn.pos[1], turn.pos[2], *changed)) {
        return;
    }
    turn.stateId = *changed;
    std::vector<PlayerSession*> watchers = players_->snapshot();
    std::erase_if(watchers, [&turn](const PlayerSession* player) { return player->worldName != turn.world.name(); });
    broadcaster_->announceRedstone(
        turn.world, watchers,
        world::BlockChange{.x = turn.pos[0], .y = turn.pos[1], .z = turn.pos[2], .stateId = *changed});
    if (*from == vaults::State::Ejecting) {
        soundAt(turn, "block.vault.close_shutter", 1.0F, 1.0F);
    }
    onEnter(turn, to);
}

void VaultSystem::onEnter(Turn& turn, const vaults::State state) {
    const std::int32_t ominous = turn.ominous() ? 1 : 0;
    switch (state) {
    case vaults::State::Inactive:
        turn.shared.displayItem.reset();
        levelEvent(turn, kDeactivateEvent, ominous);
        break;
    case vaults::State::Active:
        if (!turn.shared.displayItem.has_value()) {
            cycleDisplayItem(turn, state);
        }
        levelEvent(turn, kActivateEvent, ominous);
        break;
    case vaults::State::Unlocking:
        soundAt(turn, "block.vault.insert_item", 1.0F, 1.0F);
        break;
    case vaults::State::Ejecting:
        soundAt(turn, "block.vault.open_shutter", 1.0F, 1.0F);
        break;
    }
}

void VaultSystem::eject(Turn& turn, const ItemStack& stack, const float progress) {
    // parity: `DefaultDispenseItemBehavior.spawnItem` upward from 1.2 blocks over the vault.
    const auto triangle = [&turn](const double center, const double spread) {
        const double first = turn.random.nextDouble();
        const double second = turn.random.nextDouble();
        return center + (spread * (first - second));
    };
    const double spread = 0.0172275 * kEjectAccuracy;
    // parity: the `pow` draw that only scales the horizontal step, which is zero going up.
    static_cast<void>(turn.random.nextDouble());
    world::DroppedItem item{.entityId = players_->allocateEntityId(),
                            .uuid = Uuid::random(),
                            .item = stack.item,
                            .count = stack.count,
                            .x = turn.pos[0] + 0.5,
                            .y = turn.pos[1] + kEjectHeight - 0.125,
                            .z = turn.pos[2] + 0.5};
    item.components = stack.components;
    item.velocityX = triangle(0.0, spread);
    item.velocityY = triangle(0.2, spread);
    item.velocityZ = triangle(0.0, spread);
    static_cast<void>(turn.world.state().spawnItem(std::move(item)));
    levelEvent(turn, kEjectEvent, 0);
    soundAt(turn, "block.vault.eject_item", 1.0F, 0.8F + (0.4F * progress));
}

VaultSystem::Loot VaultSystem::roll(Turn& turn, const std::string& table, const PlayerSession* who,
                                    const ItemStack* key) {
    if (options_.lootTables == nullptr) {
        return {};
    }
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(table);
    const server::world::loot::LootTable* rolled = id.has_value() ? options_.lootTables->byId(*id) : nullptr;
    if (rolled == nullptr) {
        return {};
    }
    server::world::loot::LootContext context{.hasEntity = who != nullptr};
    options_.lootTables->prepareAt(context, turn.world, turn.pos[0] + 0.5, turn.pos[1] + 0.5, turn.pos[2] + 0.5);
    if (who != nullptr) {
        context.luck = who->luck();
        context.thisEntity =
            server::world::loot::EntityFacts{.type = Identifier::vanilla("player"), .scoreName = who->name()};
    }
    if (key != nullptr) {
        context.tool = key->item;
        context.toolComponents = key->components;
    }
    return rolled->generate(turn.random, context.luck, std::nullopt, context);
}

void VaultSystem::failSound(Turn& turn, const char* name) {
    std::int64_t& last = lastInsertFail_[{turn.world.name(), turn.pos}];
    if (turn.gameTime >= last + kFailSoundBuffer) {
        soundAt(turn, name, 1.0F, 1.0F);
        last = turn.gameTime;
    }
}

void VaultSystem::soundAt(Turn& turn, const char* name, const float volume, const float pitch) const {
    const std::int64_t seed = options_.nextSeed ? options_.nextSeed() : 0;
    const double x = turn.pos[0] + 0.5;
    const double y = turn.pos[1] + 0.5;
    const double z = turn.pos[2] + 0.5;
    const double range = static_cast<double>(std::max(volume, 1.0F)) * 16.0;
    for (const PlayerSession* player : players_->snapshot()) {
        const double dx = x - player->x;
        const double dy = y - player->y;
        const double dz = z - player->z;
        if (player->worldName == turn.world.name() && (dx * dx) + (dy * dy) + (dz * dz) < range * range) {
            player::playSoundFor(*player, name, wire::protocol::play::SoundSource::Blocks, x, y, z, volume, pitch,
                                 seed);
        }
    }
}

void VaultSystem::levelEvent(Turn& turn, const std::int32_t type, const std::int32_t data) const {
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName != turn.world.name() || !player->connection().sendLevelEvent) {
            continue;
        }
        const double dx = turn.pos[0] + 0.5 - player->x;
        const double dy = turn.pos[1] + 0.5 - player->y;
        const double dz = turn.pos[2] + 0.5 - player->z;
        if ((dx * dx) + (dy * dy) + (dz * dz) < kLevelEventRange * kLevelEventRange) {
            player->connection().sendLevelEvent(type, turn.pos[0], turn.pos[1], turn.pos[2], data);
        }
    }
}

void VaultSystem::sendUpdate(Turn& turn) const {
    const std::optional<std::int32_t> typeId =
        registry::blockEntityTypes::idOf(Identifier::vanilla(std::string{kVaultBlock}));
    if (!typeId.has_value()) {
        return;
    }
    const wire::protocol::play::BlockEntityDataPacket packet{
        .pos = {.x = turn.pos[0], .y = turn.pos[1], .z = turn.pos[2]},
        .typeId = *typeId,
        .tag = turn.shared.updateTag()};
    for (const PlayerSession* player : players_->snapshot()) {
        const double dx = turn.pos[0] + 0.5 - player->x;
        const double dz = turn.pos[2] + 0.5 - player->z;
        if (player->worldName == turn.world.name() && (dx * dx) + (dz * dz) < kUpdateRange * kUpdateRange &&
            player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::BlockEntityDataPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

bool VaultSystem::commit(Turn& turn, const vaults::ServerData& serverBefore, const vaults::SharedData& sharedBefore) {
    const bool serverChanged = !(turn.server == serverBefore);
    const bool sharedChanged = !(turn.shared == sharedBefore);
    if (!serverChanged && !sharedChanged) {
        return false;
    }
    // The state change may have rewritten the chunk's block entities, so the vault is looked up again.
    server::world::Chunk* chunk = chunkOf(turn.world, turn.pos);
    server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(turn.pos[0] & 15, turn.pos[1], turn.pos[2] & 15);
    if (entity == nullptr) {
        return false;
    }
    turn.server.write(entity->data());
    turn.shared.write(entity->data());
    if (sharedChanged) {
        sendUpdate(turn);
    }
    return true;
}

} // namespace basalt::server::runtime
