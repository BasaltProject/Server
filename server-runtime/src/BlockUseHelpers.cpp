#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerBlockItemUse.hpp>
#include <basalt/server/runtime/player/PlayerBlockSlots.hpp>
#include <basalt/server/runtime/player/PlayerBlockToggle.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSigns.hpp>
#include <basalt/server/runtime/player/PlayerSleep.hpp>
#include <basalt/server/runtime/player/PlayerSpawnItems.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/BlockPlacement.hpp>
#include <basalt/server/runtime/world/Buckets.hpp>
#include <basalt/server/runtime/world/Campfires.hpp>
#include <basalt/server/runtime/world/ChiseledBookshelves.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Composters.hpp>
#include <basalt/server/runtime/world/DaylightDetectors.hpp>
#include <basalt/server/runtime/world/DecoratedPots.hpp>
#include <basalt/server/runtime/world/Explosions.hpp>
#include <basalt/server/runtime/world/ItemFrames.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>
#include <basalt/server/runtime/world/Lecterns.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Rails.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>
#include <basalt/server/runtime/world/Signs.hpp>
#include <basalt/server/runtime/world/SnowyBlocks.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;

using play::BlockPos;
using play::Gamemode;
using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;

} // namespace

void BlockInteractionHandler::broadcastSound(const BlockPos& pos, const std::string& sound, float volume, float pitch) {
    const play::SoundPacket packet{.sound = sound,
                                   .source = play::SoundSource::Blocks,
                                   .x = pos.x + 0.5,
                                   .y = pos.y + 0.5,
                                   .z = pos.z + 0.5,
                                   .volume = volume,
                                   .pitch = pitch,
                                   .seed = nextSeed()};
    owner_->sendPlayPacket(play::SoundPacket::kId, [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    const PlayerSession* session = owner_->playerSession();
    for (const PlayerSession* other : owner_->playersInSameWorld()) {
        if (session != nullptr && other->entityId() == session->entityId()) {
            continue;
        }
        // A door, a chest or a button on the other edition: told by name, since that client has no
        // packet of this shape and hears nothing at all without it.
        if (other->connection().playSound) {
            other->connection().playSound(sound, pos.x + 0.5, pos.y + 0.5, pos.z + 0.5, volume, pitch);
            continue;
        }
        if (!other->connection().sendPlayPacket) {
            continue;
        }
        other->connection().sendPlayPacket(play::SoundPacket::kId,
                                           [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    }
}

bool BlockInteractionHandler::ringBell(const play::BlockHitResult& hit) {
    world::LoadedWorld* world = owner_->currentWorld();
    return world != nullptr &&
           player::ringBellFor(breakContext(), *world, hit.pos.x, hit.pos.y, hit.pos.z, hit.direction, hit.clickY);
}

void BlockInteractionHandler::openNewSign(const BlockPos& pos, std::int32_t stateId) {
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<Identifier> type = world::signs::typeOf(stateId);
    server::world::Chunk* chunk = world == nullptr ? nullptr : world->state().chunkIfLoaded(pos.x >> 4, pos.z >> 4);
    if (!type.has_value() || chunk == nullptr) {
        return;
    }
    chunk->setBlockEntity(world::signs::blockEntity(pos.x & 15, pos.y, pos.z & 15, *type));
    owner_->sendCompressed(play::OpenSignEditorPacket::kId, [&pos](wire::io::ByteBuffer& buffer) {
        play::OpenSignEditorPacket{.pos = pos, .isFrontText = true}.write(buffer);
    });
}

bool BlockInteractionHandler::editSign(const BlockPos& pos) {
    world::LoadedWorld* world = owner_->currentWorld();
    server::world::Chunk* chunk = world == nullptr ? nullptr : world->state().chunkIfLoaded(pos.x >> 4, pos.z >> 4);
    const server::world::ChunkBlockEntity* existing =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(pos.x & 15, pos.y, pos.z & 15);
    if (existing == nullptr || world::signs::isWaxed(existing->data())) {
        return false;
    }
    owner_->sendCompressed(play::OpenSignEditorPacket::kId, [&pos](wire::io::ByteBuffer& buffer) {
        play::OpenSignEditorPacket{.pos = pos, .isFrontText = true}.write(buffer);
    });
    return true;
}

void BlockInteractionHandler::writeSign(const play::SignUpdatePacket& update) {
    world::LoadedWorld* world = owner_->currentWorld();
    PlayerSession* session = owner_->playerSession();
    if (world == nullptr || session == nullptr) {
        return;
    }
    static_cast<void>(player::writeSignFor(breakContext(), *world, *session, update.pos.x, update.pos.y, update.pos.z,
                                           update.isFrontText,
                                           std::vector<std::string>(update.lines.begin(), update.lines.end()),
                                           [this](PlayerSession& who, const std::int32_t x, const std::int32_t y,
                                                  const std::int32_t z, std::vector<std::string>& lines) {
                                               return !hooks_.signChange || hooks_.signChange(who, x, y, z, lines);
                                           }));
}

bool BlockInteractionHandler::mayUseGameMasterBlocks(PlayerSession& session) const {
    // parity: `Player.canUseGameMasterBlocks`, which is permission two **and** creative. Here the
    // permission is the admin group, which is what a player's command source resolves to.
    const command::CommandDispatcher* dispatcher = owner_->options().commands;
    if (dispatcher == nullptr || session.gamemode != Gamemode::Creative) {
        return false;
    }
    // Asked through the player's own command source, so "operator" means here exactly what it means
    // when they type a command.
    const command::PlayerCommandSource source{session, dispatcher->environment().groups()};
    return source.permissionLevel() >= command::permissionLevel::kGamemasters;
}

void BlockInteractionHandler::programCommandBlock(const play::SetCommandBlockPacket& packet) {
    world::LoadedWorld* world = owner_->currentWorld();
    PlayerSession* session = owner_->playerSession();
    if (world == nullptr || session == nullptr) {
        return;
    }
    if (!mayUseGameMasterBlocks(*session)) {
        session->sendSystemMessage(wire::text::Component::translatable("advMode.notAllowed"));
        return;
    }
    using PacketMode = play::SetCommandBlockPacket::Mode;
    const world::commandBlocks::Mode mode = packet.mode == PacketMode::Sequence ? world::commandBlocks::Mode::Chain
                                            : packet.mode == PacketMode::Auto   ? world::commandBlocks::Mode::Repeating
                                                                                : world::commandBlocks::Mode::Impulse;
    const std::optional<world::BlockChange> moved =
        world::commandBlocks::program(world->state(), packet.pos.x, packet.pos.y, packet.pos.z, packet.command, mode,
                                      packet.trackOutput, packet.conditional, packet.automatic);
    if (moved.has_value()) {
        broadcastBlockUpdate(BlockPos{moved->x, moved->y, moved->z}, moved->stateId);
    }
    sendBlockEntity(packet.pos);
    // parity: the line is echoed back to whoever typed it, and says so when the rule is off.
    if (!packet.command.empty()) {
        const bool works = world->gameRules().get(world::gameRules::kCommandBlocksWork);
        session->sendSystemMessage(
            wire::text::Component::translatable(works ? "advMode.setCommand.success" : "advMode.setCommand.disabled",
                                                {wire::text::Component::text(packet.command)}));
    }
}

void BlockInteractionHandler::sendBlockEntity(const BlockPos& pos) {
    world::LoadedWorld* world = owner_->currentWorld();
    server::world::Chunk* chunk = world == nullptr ? nullptr : world->state().chunkIfLoaded(pos.x >> 4, pos.z >> 4);
    const server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(pos.x & 15, pos.y, pos.z & 15);
    const std::optional<std::int32_t> typeId =
        entity == nullptr ? std::nullopt : reg::blockEntityTypes::idOf(entity->type());
    if (!typeId.has_value()) {
        return;
    }
    const play::BlockEntityDataPacket packet{.pos = pos, .typeId = *typeId, .tag = entity->data()};
    for (const PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(play::BlockEntityDataPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

bool BlockInteractionHandler::useLectern(PlayerSession& session, const BlockPos& pos) {
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<std::int32_t> stateId =
        world == nullptr ? std::nullopt : world->state().blockState(pos.x, pos.y, pos.z);
    if (!stateId.has_value()) {
        return false;
    }
    if (reg::blockStateDefinitions::propertiesOf(*stateId)["has_book"] == "true") {
        openLectern(session, pos);
        return true;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    return player::placeBookOnLecternFor(breakContext(), session, *world, pos.x, pos.y, pos.z, heldSlot,
                                         session.inventory.at(heldSlot));
}

void BlockInteractionHandler::openLectern(PlayerSession& session, const BlockPos& pos) {
    ContainerMenuController& menus = owner_->menus();
    menus.returnCraftingGrid(session);
    menus.nextContainerId = menus.nextContainerId >= ClientSession::kMaxContainerId
                                ? ClientSession::kFirstBlockContainerId
                                : menus.nextContainerId + 1;
    menus.openMenu = player::Menu::playerInventory();
    menus.openBlock = pos;
    menus.openLecternAt = pos;
    const std::int32_t containerId = menus.nextContainerId;
    owner_->sendCompressed(play::OpenScreenPacket::kId, [containerId](wire::io::ByteBuffer& buffer) {
        play::OpenScreenPacket{.containerId = containerId,
                               .menuType = play::menuType::kLectern,
                               .title = wire::text::Component::translatable("block.minecraft.lectern")}
            .write(buffer);
    });
}

bool BlockInteractionHandler::useJukebox(PlayerSession& session, const BlockPos& pos) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return false;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    return player::useJukeboxFor(breakContext(), session, *world, pos.x, pos.y, pos.z, heldSlot,
                                 session.inventory.at(heldSlot));
}

bool BlockInteractionHandler::hangFrame(PlayerSession& session, const play::BlockHitResult& hit,
                                        const std::int32_t heldSlot, const storage::StoredItemStack& held,
                                        const Identifier& type) {
    world::LoadedWorld* world = owner_->currentWorld();
    return world != nullptr && player::hangFrameFor(breakContext(), session, *world, type, heldSlot, held,
                                                    player::BlockHit{.x = hit.pos.x,
                                                                     .y = hit.pos.y,
                                                                     .z = hit.pos.z,
                                                                     .direction = hit.direction,
                                                                     .clickX = hit.clickX,
                                                                     .clickY = hit.clickY,
                                                                     .clickZ = hit.clickZ});
}

bool BlockInteractionHandler::placeMinecart(PlayerSession& session, const Identifier& item, std::int32_t heldSlot,
                                            const storage::StoredItemStack& held, const play::BlockHitResult& hit) {
    world::LoadedWorld* world = owner_->currentWorld();
    return world != nullptr && player::placeMinecartFor(breakContext(), session, *world, item, heldSlot, held,
                                                        player::BlockHit{.x = hit.pos.x,
                                                                         .y = hit.pos.y,
                                                                         .z = hit.pos.z,
                                                                         .direction = hit.direction,
                                                                         .clickX = hit.clickX,
                                                                         .clickY = hit.clickY,
                                                                         .clickZ = hit.clickZ});
}

bool BlockInteractionHandler::placeBoat(PlayerSession& session, const Identifier& item, std::int32_t heldSlot,
                                        const storage::StoredItemStack& held, const play::BlockHitResult& hit) {
    world::LoadedWorld* world = owner_->currentWorld();
    return world != nullptr && player::placeBoatFor(breakContext(), session, *world, item, heldSlot, held,
                                                    player::BlockHit{.x = hit.pos.x,
                                                                     .y = hit.pos.y,
                                                                     .z = hit.pos.z,
                                                                     .direction = hit.direction,
                                                                     .clickX = hit.clickX,
                                                                     .clickY = hit.clickY,
                                                                     .clickZ = hit.clickZ});
}

bool BlockInteractionHandler::useSpawnEgg(PlayerSession& session, const Identifier& egg, std::int32_t heldSlot,
                                          const storage::StoredItemStack& held, const play::BlockHitResult& hit) {
    world::LoadedWorld* world = owner_->currentWorld();
    return world != nullptr && player::useSpawnEggFor(breakContext(), session, *world, egg, heldSlot, held,
                                                      player::BlockHit{.x = hit.pos.x,
                                                                       .y = hit.pos.y,
                                                                       .z = hit.pos.z,
                                                                       .direction = hit.direction,
                                                                       .clickX = hit.clickX,
                                                                       .clickY = hit.clickY,
                                                                       .clickZ = hit.clickZ});
}

void BlockInteractionHandler::useBookshelf(PlayerSession& session, const play::BlockHitResult& hit) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    player::useBookshelfFor(breakContext(), session, *world, hit.pos.x, hit.pos.y, hit.pos.z,
                            player::FaceHit{.face = hit.direction, .x = hit.clickX, .y = hit.clickY, .z = hit.clickZ},
                            heldSlot, session.inventory.at(heldSlot), owner_->options().itemTags);
}

void BlockInteractionHandler::useShelf(PlayerSession& session, const play::BlockHitResult& hit) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    player::useShelfFor(breakContext(), session, *world, hit.pos.x, hit.pos.y, hit.pos.z,
                        player::FaceHit{.face = hit.direction, .x = hit.clickX, .y = hit.clickY, .z = hit.clickZ},
                        heldSlot, session.inventory.at(heldSlot));
}

void BlockInteractionHandler::usePot(PlayerSession& session, const BlockPos& pos) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    player::usePotFor(breakContext(), session, *world, pos.x, pos.y, pos.z, heldSlot, session.inventory.at(heldSlot));
}

bool BlockInteractionHandler::placeOnCampfire(PlayerSession& session, const BlockPos& pos) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return false;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    return player::placeOnCampfireFor(breakContext(), session, *world, pos.x, pos.y, pos.z, heldSlot,
                                      session.inventory.at(heldSlot), owner_->options().recipes);
}

bool BlockInteractionHandler::useComposter(PlayerSession& session, const BlockPos& pos) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return false;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    return player::useComposterFor(breakContext(), session, *world, pos.x, pos.y, pos.z, heldSlot,
                                   session.inventory.at(heldSlot));
}

void BlockInteractionHandler::notifyComparators(const BlockPos& pos) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    world::redstoneComponents::scheduleComparatorsAround(world->state(), world->name(), pos.x, pos.y, pos.z,
                                                         world::comparatorReading::of(world->state()));
}

void BlockInteractionHandler::refreshSnowUnder(const BlockPos& pos) {
    refreshSnowAt(BlockPos{pos.x, pos.y - 1, pos.z});
}

void BlockInteractionHandler::refreshSnowAt(const BlockPos& pos) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    const std::optional<std::int32_t> updated = world::snowyBlocks::updated(world->state(), pos.x, pos.y, pos.z);
    if (!updated.has_value() || !world->state().setBlockState(pos.x, pos.y, pos.z, *updated)) {
        return;
    }
    broadcastBlockUpdate(pos, *updated);
}

void BlockInteractionHandler::useBucketOn(PlayerSession& session, const Identifier& item, std::int32_t slot,
                                          const storage::StoredItemStack& held, const play::BlockHitResult& hit) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    const world::fluidFlow::Context context{.world = world->state(),
                                            .gameRules = world->gameRules(),
                                            .random = random,
                                            .ultrawarm =
                                                world->declaration().dimension == config::WorldDimension::Nether};
    const std::optional<world::buckets::Result> result = world::buckets::useOn(
        context, item, hit.pos.x, hit.pos.y, hit.pos.z, hit.direction, session.gamemode == Gamemode::Creative);
    if (!result.has_value()) {
        return;
    }
    // parity: `BucketItem` raises `FLUID_PICKUP` where it took the fluid from and `FLUID_PLACE`
    // where it put it, and a sculk sensor hears both. Which of the two it is comes from the item:
    // an empty bucket is the one that picks up.
    if (!result->changes.empty()) {
        const world::FluidChange& at = result->changes.front();
        if (world->state().hasSculkSensorNear(at.x, at.y, at.z, world::vibrations::kCalibratedRange)) {
            world::vibrations::emit(world->state(), item.path() == "bucket" ? "fluid_pickup" : "fluid_place",
                                    at.x + 0.5, at.y + 0.5, at.z + 0.5, world->gameTime(), {.entity = session.uuid()});
        }
    }
    for (const world::FluidChange& change : result->changes) {
        broadcastBlockUpdate(BlockPos{change.x, change.y, change.z}, change.stateId);
    }
    if (!result->replacement.has_value()) {
        return;
    }
    session.inventory.set(slot, storage::StoredItemStack{
                                    .id = result->replacement->toString(), .count = 1, .components = held.components});
    owner_->sendInventorySlot(slot);
}

std::vector<std::array<double, 6>> BlockInteractionHandler::buildersInTheWay() const {
    const PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return {};
    }
    std::vector<const PlayerSession*> players;
    bool sawSelf = false;
    for (const PlayerSession* player : owner_->playersInSameWorld()) {
        sawSelf = sawSelf || player->entityId() == session->entityId();
        players.push_back(player);
    }
    if (!sawSelf) {
        players.push_back(session);
    }
    std::vector<std::array<double, 6>> boxes;
    for (const PlayerSession* player : players) {
        // parity: `Entity.blocksBuilding`, which is off for spectators, who are built straight through.
        if (player->gamemode == Gamemode::Spectator) {
            continue;
        }
        boxes.push_back(world::blockPlacement::playerBox(player->x, player->y, player->z, player->sneaking));
    }
    return boxes;
}

void BlockInteractionHandler::toggleBlock(const BlockPos& pos, const float playerYaw) {
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        player::toggleBlockFor(breakContext(), *world, pos.x, pos.y, pos.z, playerYaw);
    }
}

void BlockInteractionHandler::cycleBlock(const BlockPos& pos) {
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        player::cycleBlockFor(breakContext(), *world, pos.x, pos.y, pos.z);
    }
}

void BlockInteractionHandler::setSpawnPointAt(PlayerSession& session, const BlockPos& bed) {
    storage::PlayerStore* store = owner_->options().playerStore;
    if (store == nullptr || !store->isOpen()) {
        return;
    }
    store->access().setSpawnPoint(storage::StoredSpawnPoint{.uuid = session.uuid().toString(),
                                                            .world = session.worldName,
                                                            .x = bed.x + 0.5,
                                                            .y = static_cast<double>(bed.y),
                                                            .z = bed.z + 0.5,
                                                            .yaw = session.yaw});
    owner_->sendActionBar(wire::text::Component::translatable("block.minecraft.set_spawn"));
}

player::SleepContext BlockInteractionHandler::sleepContext() {
    player::SleepContext context;
    context.blocks = breakContext();
    context.setSpawn = [this](PlayerSession& player, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
        setSpawnPointAt(player, BlockPos{x, y, z});
    };
    context.bedAllowed = hooks_.bedAllowed;
    context.explode = [this](world::LoadedWorld& world, const double x, const double y, const double z,
                             const std::int32_t byEntityId) {
        if (ExplosionSystem* explosions = owner_->options().explosions; explosions != nullptr) {
            explosions->blowUp(world, owner_->playersInSameWorld(), x, y, z, world::explosions::kBedPower, "bed",
                               byEntityId);
        }
    };
    return context;
}

void BlockInteractionHandler::useBed(PlayerSession& session, const BlockPos& pos, std::int32_t state) {
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        static_cast<void>(player::sleepInBedFor(sleepContext(), session, *world, pos.x, pos.y, pos.z, state));
    }
}

void BlockInteractionHandler::wakeUp() {
    if (PlayerSession* session = owner_->playerSession(); session != nullptr) {
        player::wakeUpFor(breakContext(), *session, owner_->currentWorld());
    }
}

} // namespace basalt::server::runtime
