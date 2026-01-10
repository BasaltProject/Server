#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/BlockPlacement.hpp>
#include <basalt/server/runtime/world/Buckets.hpp>
#include <basalt/server/runtime/world/Campfires.hpp>
#include <basalt/server/runtime/world/ChiseledBookshelves.hpp>
#include <basalt/server/runtime/world/DecoratedPots.hpp>
#include <basalt/server/runtime/world/Explosions.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>
#include <basalt/server/runtime/world/Lecterns.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Rails.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>
#include <basalt/server/runtime/world/Signs.hpp>
#include <basalt/server/runtime/world/SnowyBlocks.hpp>
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

/// parity: `BellBlock.isProperHit`, whose ceiling for a side hit is the block's own upper edge.
constexpr float kBellHitTop = 0.8124F;

/// parity: `MinecartItem.useOn`, a sixteenth of a block above the rail.
constexpr double kMinecartRestHeight = 0.0625;

std::optional<Identifier> parsedOrNothing(const std::string& id) {
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

std::string lowercaseName(play::Direction face) {
    switch (face) {
    case play::Direction::Down:
        return "down";
    case play::Direction::Up:
        return "up";
    case play::Direction::North:
        return "north";
    case play::Direction::South:
        return "south";
    case play::Direction::West:
        return "west";
    case play::Direction::East:
        return "east";
    }
    return "north";
}

/// parity: the two axes a bell can swing along, which is what `isProperHit` compares.
std::string axisOf(std::string_view direction) { return direction == "north" || direction == "south" ? "z" : "x"; }

/// parity: `ItemFrame`'s own direction numbering, which a block event reuses for the bell.
std::int32_t frameDirectionData(std::string_view facing) {
    if (facing == "south") {
        return 0;
    }
    if (facing == "west") {
        return 1;
    }
    return facing == "north" ? 2 : 3;
}

storage::StoredItemStack withCount(const storage::StoredItemStack& stack, std::int32_t count) {
    storage::StoredItemStack copy = stack;
    copy.count = count;
    return copy;
}

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
        if ((session != nullptr && other->entityId() == session->entityId()) || !other->connection().sendPlayPacket) {
            continue;
        }
        other->connection().sendPlayPacket(play::SoundPacket::kId,
                                           [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    }
}

bool BlockInteractionHandler::ringBell(const play::BlockHitResult& hit) {
    const std::optional<std::int32_t> stateId = owner_->world().blockState(hit.pos.x, hit.pos.y, hit.pos.z);
    if (!stateId.has_value() || hit.direction == play::Direction::Up || hit.direction == play::Direction::Down ||
        hit.clickY > kBellHitTop) {
        return false;
    }
    const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(*stateId);
    const auto facing = properties.find("facing");
    const bool sameAxis =
        axisOf(facing == properties.end() ? "north" : facing->second) == axisOf(lowercaseName(hit.direction));
    const auto attachment = properties.find("attachment");
    const std::string mounting = attachment == properties.end() ? std::string{} : attachment->second;
    const bool proper = mounting == "ceiling" || (mounting == "floor" ? sameAxis : !sameAxis);
    if (!proper) {
        return false;
    }
    broadcastEffects(
        hit.pos, *stateId,
        {world::blockEffect::Event{.b0 = 1, .b1 = frameDirectionData(lowercaseName(hit.direction))},
         world::blockEffect::Sound{
             .name = "block.bell.use", .source = play::SoundSource::Blocks, .volume = 2.0F, .pitch = 1.0F}});
    return true;
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
    const BlockPos pos = update.pos;
    const std::optional<std::int32_t> stateId = world->state().blockState(pos.x, pos.y, pos.z);
    const std::optional<Identifier> type = stateId.has_value() ? world::signs::typeOf(*stateId) : std::nullopt;
    server::world::Chunk* chunk = world->state().chunkIfLoaded(pos.x >> 4, pos.z >> 4);
    if (!type.has_value() || chunk == nullptr) {
        return;
    }
    server::world::ChunkBlockEntity* existing = chunk->blockEntityAt(pos.x & 15, pos.y, pos.z & 15);
    if (existing != nullptr && world::signs::isWaxed(existing->data())) {
        return;
    }
    std::vector<std::string> lines(update.lines.begin(), update.lines.end());
    if (hooks_.signChange && !hooks_.signChange(*session, pos.x, pos.y, pos.z, lines)) {
        return;
    }
    const wire::nbt::NbtCompound data = world::signs::withText(
        existing == nullptr ? world::signs::emptyData() : existing->data(), update.isFrontText, lines);
    chunk->setBlockEntity(world::signs::blockEntity(pos.x & 15, pos.y, pos.z & 15, *type, data));
    const std::optional<std::int32_t> typeId = reg::blockEntityTypes::idOf(*type);
    if (!typeId.has_value()) {
        return;
    }
    const play::BlockEntityDataPacket packet{.pos = pos, .typeId = *typeId, .tag = data};
    for (const PlayerSession* player : owner_->playersInSameWorld()) {
        if (!player->connection().sendPlayPacket) {
            continue;
        }
        player->connection().sendPlayPacket(play::BlockEntityDataPacket::kId,
                                            [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
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
    const std::optional<storage::StoredItemStack> held = session.inventory.at(heldSlot);
    const std::optional<Identifier> item = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    if (!item.has_value() || !world::lecterns::isBook(*item)) {
        return false;
    }
    const std::optional<world::BlockChange> change =
        world::lecterns::placeBook(world->state(), pos.x, pos.y, pos.z, *item);
    if (!change.has_value()) {
        return false;
    }
    broadcastBlockUpdate(BlockPos{change->x, change->y, change->z}, change->stateId);
    session.inventory.set(heldSlot, held->count <= 1 ? std::nullopt : std::optional{withCount(*held, held->count - 1)});
    owner_->sendInventorySlot(heldSlot);
    notifyNeighbours(pos);
    return true;
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
    const std::optional<std::pair<world::BlockChange, Identifier>> ejected =
        world::jukeboxes::eject(world->state(), pos.x, pos.y, pos.z);
    if (ejected.has_value()) {
        const world::BlockChange& change = ejected->first;
        broadcastBlockUpdate(BlockPos{change.x, change.y, change.z}, change.stateId);
        spawnDroppedItem(ejected->second, 1, pos.x + 0.5, pos.y + 1.0, pos.z + 0.5);
        notifyNeighbours(pos);
        return true;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    const std::optional<storage::StoredItemStack> held = session.inventory.at(heldSlot);
    const std::optional<Identifier> item = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    if (!item.has_value() || !world::jukeboxes::isDisc(*item)) {
        return false;
    }
    world::jukeboxes::SongOutputs* songs = owner_->options().jukeboxes;
    const std::int32_t output = songs == nullptr ? 0 : songs->of(world::jukeboxes::songOf(*item));
    const std::optional<world::BlockChange> change =
        world::jukeboxes::insert(world->state(), pos.x, pos.y, pos.z, *item, output);
    if (!change.has_value()) {
        return false;
    }
    broadcastBlockUpdate(BlockPos{change->x, change->y, change->z}, change->stateId);
    if (session.gamemode != Gamemode::Creative) {
        session.inventory.set(heldSlot,
                              held->count <= 1 ? std::nullopt : std::optional{withCount(*held, held->count - 1)});
        owner_->sendInventorySlot(heldSlot);
    }
    notifyNeighbours(pos);
    return true;
}

bool BlockInteractionHandler::spawnFromItem(PlayerSession& session, const Identifier& type, std::int32_t heldSlot,
                                            const storage::StoredItemStack& held, double x, double y, double z) {
    world::LoadedWorld* world = owner_->currentWorld();
    const reg::EntityTypeEntry* entry = reg::entityTypes::of(type);
    player::PlayerRegistry* registry = owner_->options().playerRegistry;
    if (world == nullptr || entry == nullptr || registry == nullptr) {
        return false;
    }
    world->state().spawnEntity(std::make_unique<entity::ServerEntity>(
        registry->allocateEntityId(), wire::common::Uuid::random(), type, entry->protocolId, x, y, z));
    if (session.gamemode != Gamemode::Creative) {
        session.inventory.set(heldSlot,
                              held.count <= 1 ? std::nullopt : std::optional{withCount(held, held.count - 1)});
        owner_->sendInventorySlot(heldSlot);
    }
    return true;
}

bool BlockInteractionHandler::placeMinecart(PlayerSession& session, const Identifier& item, std::int32_t heldSlot,
                                            const storage::StoredItemStack& held, const play::BlockHitResult& hit) {
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<std::int32_t> rail =
        world == nullptr ? std::nullopt : world->state().blockState(hit.pos.x, hit.pos.y, hit.pos.z);
    if (!rail.has_value() || !world::rails::isRail(*rail)) {
        return false;
    }
    const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(*rail);
    const auto shape = properties.find("shape");
    if (shape == properties.end()) {
        return false;
    }
    const double slope = shape->second.starts_with("ascending") ? 0.5 : 0.0;
    // The item and the entity share a name except for the plain cart, whose entity keeps the suffix.
    const std::string path{item.path()};
    const Identifier type =
        path == "minecart"
            ? item
            : Identifier::vanilla(path.substr(0, path.size() - std::string_view{"_minecart"}.size()) + "_minecart");
    return spawnFromItem(session, type, heldSlot, held, hit.pos.x + 0.5, hit.pos.y + kMinecartRestHeight + slope,
                         hit.pos.z + 0.5);
}

bool BlockInteractionHandler::placeBoat(PlayerSession& session, const Identifier& item, std::int32_t heldSlot,
                                        const storage::StoredItemStack& held, const play::BlockHitResult& hit) {
    // parity: `getBoat` puts it at the hit itself, which is the face of the block clicked.
    const BlockPos at = hit.pos.offset(hit.direction);
    return spawnFromItem(session, item, heldSlot, held, at.x + 0.5, static_cast<double>(at.y), at.z + 0.5);
}

bool BlockInteractionHandler::useSpawnEgg(PlayerSession& session, const Identifier& egg, std::int32_t heldSlot,
                                          const storage::StoredItemStack& held, const play::BlockHitResult& hit) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return false;
    }
    const std::string path{egg.path()};
    const Identifier type = Identifier::vanilla(path.substr(0, path.size() - std::string_view{"_spawn_egg"}.size()));
    const std::optional<std::int32_t> clicked = world->state().blockState(hit.pos.x, hit.pos.y, hit.pos.z);
    if (reg::entityTypes::of(type) == nullptr || !clicked.has_value()) {
        return false;
    }
    // parity: the mob lands on the face that was clicked, unless the block has no collision at all.
    const BlockPos at = reg::blockShapes::isEmptyShape(*clicked) ? hit.pos : hit.pos.offset(hit.direction);
    return spawnFromItem(session, type, heldSlot, held, at.x + 0.5, static_cast<double>(at.y), at.z + 0.5);
}

void BlockInteractionHandler::giveOrDrop(PlayerSession& session, const storage::StoredItemStack& stack,
                                         const BlockPos& pos) {
    const std::int32_t leftover = session.inventory.add(stack);
    owner_->sendInventoryAll();
    const std::optional<Identifier> item = parsedOrNothing(stack.id);
    if (leftover <= 0 || !item.has_value()) {
        return;
    }
    spawnDroppedItem(*item, leftover, pos.x + 0.5, pos.y + 0.5, pos.z + 0.5);
}

void BlockInteractionHandler::useBookshelf(PlayerSession& session, const play::BlockHitResult& hit) {
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<std::int32_t> stateId =
        world == nullptr ? std::nullopt : world->state().blockState(hit.pos.x, hit.pos.y, hit.pos.z);
    if (!stateId.has_value()) {
        return;
    }
    const std::optional<std::int32_t> slot =
        world::chiseledBookshelves::slotAt(*stateId, lowercaseName(hit.direction), hit.clickX, hit.clickY, hit.clickZ);
    if (!slot.has_value()) {
        return;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    const std::optional<storage::StoredItemStack> held = session.inventory.at(heldSlot);
    const std::optional<Identifier> book = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    const world::realItemTags::TagMembers* tags = owner_->options().itemTags;
    const bool isBook =
        book.has_value() && tags != nullptr && world::realItemTags::contains(*tags, "minecraft:bookshelf_books", *book);
    const std::optional<world::chiseledBookshelves::Interaction> interaction =
        isBook ? world::chiseledBookshelves::putBook(world->state(), hit.pos.x, hit.pos.y, hit.pos.z, *slot, *book)
               : world::chiseledBookshelves::takeBook(world->state(), hit.pos.x, hit.pos.y, hit.pos.z, *slot);
    if (!interaction.has_value()) {
        return;
    }
    broadcastBlockUpdate(hit.pos, interaction->change.stateId);
    broadcastEffects(hit.pos, interaction->change.stateId, interaction->change.effects);
    if (interaction->taken.has_value()) {
        giveOrDrop(session, storage::StoredItemStack{.id = interaction->taken->toString(), .count = 1}, hit.pos);
    } else if (held.has_value() && session.gamemode != Gamemode::Creative) {
        session.inventory.set(heldSlot,
                              held->count <= 1 ? std::nullopt : std::optional{withCount(*held, held->count - 1)});
        owner_->sendInventorySlot(heldSlot);
    }
}

void BlockInteractionHandler::useShelf(PlayerSession& session, const play::BlockHitResult& hit) {
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<std::int32_t> stateId =
        world == nullptr ? std::nullopt : world->state().blockState(hit.pos.x, hit.pos.y, hit.pos.z);
    if (!stateId.has_value()) {
        return;
    }
    const std::optional<std::int32_t> slot =
        world::shelves::slotAt(*stateId, lowercaseName(hit.direction), hit.clickX, hit.clickY, hit.clickZ);
    if (!slot.has_value()) {
        return;
    }
    if (reg::blockStateDefinitions::propertiesOf(*stateId)["powered"] == "true") {
        swapHotbarWithShelf(session, hit.pos, *stateId);
        return;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    const std::optional<storage::StoredItemStack> held = session.inventory.at(heldSlot);
    std::optional<server::world::loot::ItemStack> placed;
    if (held.has_value()) {
        const std::optional<Identifier> item = parsedOrNothing(held->id);
        if (item.has_value()) {
            placed = server::world::loot::ItemStack{.item = *item, .count = held->count};
        }
    }
    const std::optional<world::shelves::SwapResult> result =
        world::shelves::swap(world->state(), hit.pos.x, hit.pos.y, hit.pos.z, *slot, placed);
    if (!result.has_value()) {
        return;
    }
    session.inventory.set(heldSlot, result->removed.has_value()
                                        ? std::optional{storage::StoredItemStack{.id = result->removed->item.toString(),
                                                                                 .count = result->removed->count}}
                                        : std::nullopt);
    owner_->sendInventorySlot(heldSlot);
    broadcastEffects(hit.pos, *stateId, result->effects);
}

void BlockInteractionHandler::swapHotbarWithShelf(PlayerSession& session, const BlockPos& pos, std::int32_t stateId) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    world::blockInventory::Slots hotbar(PlayerInventory::kHotbarSize);
    for (std::int32_t index = 0; index < PlayerInventory::kHotbarSize; index++) {
        const std::optional<storage::StoredItemStack>& stack =
            session.inventory.at(PlayerInventory::kHotbarStart + index);
        const std::optional<Identifier> item = stack.has_value() ? parsedOrNothing(stack->id) : std::nullopt;
        if (item.has_value()) {
            hotbar[static_cast<std::size_t>(index)] =
                server::world::loot::ItemStack{.item = *item, .count = stack->count};
        }
    }
    if (!world::shelves::swapHotbar(world->state(), pos.x, pos.y, pos.z, hotbar)) {
        return;
    }
    for (std::int32_t index = 0; index < PlayerInventory::kHotbarSize; index++) {
        const std::optional<server::world::loot::ItemStack>& stack = hotbar[static_cast<std::size_t>(index)];
        session.inventory.set(PlayerInventory::kHotbarStart + index,
                              stack.has_value() ? std::optional{storage::StoredItemStack{.id = stack->item.toString(),
                                                                                         .count = stack->count}}
                                                : std::nullopt);
    }
    owner_->sendInventoryAll();
    broadcastEffects(
        pos, stateId,
        {world::blockEffect::Sound{
            .name = "block.shelf.multi_swap", .source = play::SoundSource::Blocks, .volume = 1.0F, .pitch = 1.0F}});
}

void BlockInteractionHandler::usePot(PlayerSession& session, const BlockPos& pos) {
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<std::int32_t> stateId =
        world == nullptr ? std::nullopt : world->state().blockState(pos.x, pos.y, pos.z);
    server::world::Chunk* chunk = world == nullptr ? nullptr : world->state().chunkIfLoaded(pos.x >> 4, pos.z >> 4);
    if (!stateId.has_value() || chunk == nullptr) {
        return;
    }
    server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(pos.x & 15, pos.y, pos.z & 15);
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    const std::optional<storage::StoredItemStack> held = session.inventory.at(heldSlot);
    const std::optional<Identifier> item = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    std::optional<std::vector<world::BlockEffect>> inserted;
    if (entity != nullptr && item.has_value()) {
        inserted = world::decoratedPots::insert(entity->data(), *item);
    }
    // An empty hand, a full pot or the wrong item all end the same way: the pot rocks and takes
    // nothing.
    if (!inserted.has_value()) {
        broadcastEffects(pos, *stateId, world::decoratedPots::rebuff());
        return;
    }
    chunk->markDirty();
    if (session.gamemode != Gamemode::Creative) {
        session.inventory.set(heldSlot,
                              held->count <= 1 ? std::nullopt : std::optional{withCount(*held, held->count - 1)});
        owner_->sendInventorySlot(heldSlot);
    }
    broadcastEffects(pos, *stateId, *inserted);
}

bool BlockInteractionHandler::placeOnCampfire(PlayerSession& session, const BlockPos& pos) {
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<std::int32_t> stateId =
        world == nullptr ? std::nullopt : world->state().blockState(pos.x, pos.y, pos.z);
    if (!stateId.has_value() || !world::campfires::isLitCampfire(*stateId)) {
        return false;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    const std::optional<storage::StoredItemStack> held = session.inventory.at(heldSlot);
    const std::optional<Identifier> item = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    server::world::Chunk* chunk = world->state().chunkIfLoaded(pos.x >> 4, pos.z >> 4);
    if (!item.has_value() || chunk == nullptr) {
        return false;
    }
    server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(pos.x & 15, pos.y, pos.z & 15);
    if (entity == nullptr) {
        chunk->setBlockEntity(
            server::world::ChunkBlockEntity{pos.x & 15, pos.y, pos.z & 15, world::campfires::block(), {}});
        entity = chunk->blockEntityAt(pos.x & 15, pos.y, pos.z & 15);
        if (entity == nullptr) {
            return false;
        }
    }
    world::campfires::State state = world::campfires::read(entity->data());
    world::RealRecipes* recipes = owner_->options().recipes;
    const bool placed = world::campfires::placeFood(
        state, server::world::loot::ItemStack{.item = *item, .count = 1},
        [recipes](const server::world::loot::ItemStack& food) {
            return recipes == nullptr ? nullptr : recipes->cookingFor(server::world::crafting::Cooker::Campfire, food);
        });
    if (!placed) {
        return false;
    }
    world::campfires::write(state, entity->data());
    chunk->markDirty();
    if (session.gamemode != Gamemode::Creative) {
        session.inventory.set(heldSlot,
                              held->count <= 1 ? std::nullopt : std::optional{withCount(*held, held->count - 1)});
        owner_->sendInventorySlot(heldSlot);
    }
    return true;
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

void BlockInteractionHandler::toggleBlock(const BlockPos& pos) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    const std::optional<std::int32_t> before = world->state().blockState(pos.x, pos.y, pos.z);
    const std::optional<world::blockInteractions::BlockToggle> toggled =
        world::blockInteractions::toggle(world->state(), pos.x, pos.y, pos.z);
    if (!toggled.has_value()) {
        return;
    }
    // parity: `ButtonBlock.press` schedules its own release. Everything else stays put until
    // something touches it again.
    const std::optional<std::int32_t> pressed = world::blockInteractions::pressedTicks(before.value_or(0));
    if (pressed.has_value()) {
        world->state().scheduleBlockTick(pos.x, pos.y, pos.z, Identifier::vanilla("button"), *pressed);
    }
    for (const world::BlockChange& change : toggled->changes) {
        broadcastBlockUpdate(BlockPos{change.x, change.y, change.z}, change.stateId);
    }
    // parity: `LeverBlock.updateNeighbours`. Without this the lever moved and powered nothing at all.
    for (const world::BlockChange& change : toggled->changes) {
        notifyNeighbours(BlockPos{change.x, change.y, change.z});
    }
    broadcastSound(pos, toggled->sound, 1.0F, 1.0F);
}

void BlockInteractionHandler::cycleBlock(const BlockPos& pos) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    const std::optional<world::blockInteractions::BlockCycle> cycled =
        world::blockInteractions::cycle(world->state(), pos.x, pos.y, pos.z);
    if (!cycled.has_value()) {
        return;
    }
    broadcastBlockUpdate(pos, cycled->stateId);
    // parity: both are components, so the change has to reach the dust and the diodes around them.
    notifyNeighbours(pos);
    if (cycled->sound.has_value()) {
        broadcastSound(pos, *cycled->sound, cycled->volume, cycled->pitch);
    }
}

std::optional<world::BedProblem> BlockInteractionHandler::bedReachProblem(const PlayerSession& session,
                                                                          const BlockPos& head,
                                                                          std::int32_t headState) const {
    if (world::beds::isOccupied(headState)) {
        return world::BedProblem::Occupied;
    }
    const double dx = session.x - (head.x + 0.5);
    const double dy = session.y - head.y;
    const double dz = session.z - (head.z + 0.5);
    if ((dx * dx) + (dy * dy) + (dz * dz) > world::beds::kRange * world::beds::kRange) {
        return world::BedProblem::TooFarAway;
    }
    return std::nullopt;
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

void BlockInteractionHandler::useBed(PlayerSession& session, const BlockPos& pos, std::int32_t state) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    const std::array<std::int32_t, 3> step = world::beds::headOffset(state);
    const BlockPos head =
        world::beds::isHead(state) ? pos : BlockPos{pos.x + step[0], pos.y + step[1], pos.z + step[2]};
    const std::optional<std::int32_t> headState = world->state().blockState(head.x, head.y, head.z);
    if (!headState.has_value() || !world::beds::isBed(*headState)) {
        return;
    }

    if (world::beds::explodesIn(world->declaration().dimension)) {
        for (const world::BlockChange& change : world::beds::destroy(world->state(), head.x, head.y, head.z)) {
            broadcastBlockUpdate(BlockPos{change.x, change.y, change.z}, change.stateId);
        }
        owner_->sendActionBar(wire::text::Component::translatable("block.minecraft.bed.not_valid"));
        // parity: `BedBlock.useWithoutItem`, which does not merely refuse: it takes the bed out and
        // sets off an explosion of five where it stood.
        if (ExplosionSystem* explosions = owner_->options().explosions; explosions != nullptr) {
            explosions->blowUp(*world, owner_->playersInSameWorld(), head.x + 0.5, head.y + 0.5, head.z + 0.5,
                               world::explosions::kBedPower, "bed", session.entityId());
        }
        return;
    }
    // parity: `ServerPlayer.startSleepInBed` sets the spawn point once the bed is reachable and
    // free, and only then asks whether the night allows sleeping.
    const std::optional<world::BedProblem> reachProblem = bedReachProblem(session, head, *headState);
    if (reachProblem.has_value()) {
        owner_->sendActionBar(wire::text::Component::translatable(std::string{world::translationKeyOf(*reachProblem)}));
        return;
    }
    setSpawnPointAt(session, head);
    if (!world::beds::isNight(world->clock().totalTicks()) && !world->weather().thundering) {
        owner_->sendActionBar(wire::text::Component::translatable(
            std::string{world::translationKeyOf(world::BedProblem::NotPossibleNow)}));
        return;
    }

    const std::int32_t occupied = world::beds::withOccupied(*headState, true);
    world->state().setBlockState(head.x, head.y, head.z, occupied);
    broadcastBlockUpdate(head, occupied);
    if (hooks_.bedAllowed && !hooks_.bedAllowed(session, head.x, head.y, head.z)) {
        return;
    }
    session.sleepingBed = std::array<std::int32_t, 3>{head.x, head.y, head.z};
    session.sleepTicks = 0;
    session.stats.reset(storage::stats::timeSinceRest());
    const play::SetEntityDataPacket packet = play::SetEntityDataPacket::sleeping(session.entityId(), head);
    owner_->sendPlayPacket(play::SetEntityDataPacket::kId,
                           [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    for (const PlayerSession* other : owner_->playersInSameWorld()) {
        if (other->entityId() == session.entityId() || !other->connection().sendPlayPacket) {
            continue;
        }
        other->connection().sendPlayPacket(play::SetEntityDataPacket::kId,
                                           [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    }
}

void BlockInteractionHandler::wakeUp() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr || !session->sleepingBed.has_value()) {
        return;
    }
    const std::array<std::int32_t, 3> bed = *session->sleepingBed;
    session->sleepingBed.reset();
    session->sleepTicks = 0;
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<std::int32_t> state =
        world == nullptr ? std::nullopt : world->state().blockState(bed[0], bed[1], bed[2]);
    if (world != nullptr && state.has_value() && world::beds::isBed(*state)) {
        const std::int32_t awake = world::beds::withOccupied(*state, false);
        world->state().setBlockState(bed[0], bed[1], bed[2], awake);
        broadcastBlockUpdate(BlockPos{bed[0], bed[1], bed[2]}, awake);
    }
    // Nobody was moved into the bed, so nobody moves out.
    const play::SetEntityDataPacket packet = play::SetEntityDataPacket::sleeping(session->entityId(), std::nullopt);
    owner_->sendPlayPacket(play::SetEntityDataPacket::kId,
                           [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    for (const PlayerSession* other : owner_->playersInSameWorld()) {
        if (other->entityId() == session->entityId() || !other->connection().sendPlayPacket) {
            continue;
        }
        other->connection().sendPlayPacket(play::SetEntityDataPacket::kId,
                                           [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    }
}

} // namespace basalt::server::runtime
