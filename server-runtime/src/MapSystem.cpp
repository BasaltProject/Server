#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/ItemFrames.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/maps/MapDecorationTypes.hpp>
#include <basalt/server/runtime/world/maps/MapRenderer.hpp>
#include <basalt/server/runtime/world/maps/MapStorage.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <algorithm>
#include <array>
#include <numbers>
#include <random>
#include <string>
#include <utility>

namespace basalt::server::runtime {
namespace {

namespace maps = world::maps;
namespace play = wire::protocol::play;
using player::PlayerInventory;
using player::PlayerSession;
using wire::nbt::NbtCompound;

constexpr std::string_view kMapId = "minecraft:map_id";
constexpr std::string_view kMapDecorations = "minecraft:map_decorations";
constexpr std::string_view kPostProcessing = "minecraft:map_post_processing";
constexpr std::string_view kEmptyMap = "minecraft:map";
constexpr std::string_view kFilledMap = "minecraft:filled_map";
constexpr std::string_view kTakeResultSound = "minecraft:ui.cartography_table.take_result";
constexpr std::string_view kInvisibilityTag = "minecraft:map_invisibility_equipment";
/// parity: `ServerEntity.sendChanges`, which looks at a framed map every tenth tick.
constexpr std::int64_t kFrameInterval = 10;

/// parity: the items `Items` registers as `MapItem`.
constexpr std::array<std::string_view, 17> kMapItems{"minecraft:filled_map",
                                                     "minecraft:ocean_monument_map",
                                                     "minecraft:woodland_mansion_map",
                                                     "minecraft:buried_trial_chambers_map",
                                                     "minecraft:jungle_pyramid_map",
                                                     "minecraft:swamp_hut_map",
                                                     "minecraft:desert_village_map",
                                                     "minecraft:plains_village_map",
                                                     "minecraft:savanna_village_map",
                                                     "minecraft:snowy_village_map",
                                                     "minecraft:taiga_village_map",
                                                     "minecraft:buried_treasure_map",
                                                     "minecraft:buried_ancient_city_map",
                                                     "minecraft:buried_mineshaft_map",
                                                     "minecraft:desert_pyramid_map",
                                                     "minecraft:abandoned_camp_map",
                                                     "minecraft:warm_ocean_ruins_map"};

/// Vanilla's inventory order, hotbar first, mapped onto this server's slots.
[[nodiscard]] std::int32_t vanillaMainSlot(std::int32_t index) {
    return index < PlayerInventory::kHotbarSize ? PlayerInventory::kHotbarStart + index
                                                : index - PlayerInventory::kHotbarSize;
}

/// parity: `EntityEquipment`'s enum order after the main hand: off hand, feet, legs, chest, head.
constexpr std::array<std::int32_t, 5> kEquipmentTickOrder{PlayerInventory::kOffhandSlot, 39, 38, 37, 36};
/// parity: `Inventory.EQUIPMENT_SLOT_MAPPING`: feet, legs, chest, head, off hand.
constexpr std::array<std::int32_t, 5> kEquipmentSyncOrder{39, 38, 37, 36, PlayerInventory::kOffhandSlot};

/// parity: `Direction.get2DDataValue` of a frame's `Facing`, which is `Direction`'s 3D id.
[[nodiscard]] std::int32_t twoDimensionalValue(std::int32_t facing) {
    constexpr std::array<std::int32_t, 6> kValues{-1, -1, 2, 0, 1, 3};
    return facing >= 0 && facing < 6 ? kValues[static_cast<std::size_t>(facing)] : -1;
}

[[nodiscard]] std::optional<maps::MapBanner> bannerAt(const world::LoadedWorld& world, const maps::BlockPos& pos) {
    const server::world::Chunk* chunk = world.state().chunkIfLoaded(pos[0] >> 4, pos[2] >> 4);
    if (chunk == nullptr) {
        return std::nullopt;
    }
    const server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(pos[0] & 15, pos[1], pos[2] & 15);
    const wire::common::Identifier* block =
        registry::blockStateDefinitions::blockOf(chunk->blockStateId(pos[0] & 15, pos[1], pos[2] & 15));
    if (entity == nullptr || block == nullptr) {
        return std::nullopt;
    }
    std::string_view name = block->path();
    for (const std::string_view suffix : {std::string_view{"_wall_banner"}, std::string_view{"_banner"}}) {
        if (name.ends_with(suffix)) {
            name.remove_suffix(suffix.size());
            maps::MapBanner banner{.pos = pos, .color = std::string{name}};
            if (const wire::nbt::NbtTag* custom = entity->data().find("CustomName"); custom != nullptr) {
                banner.name = wire::text::componentNbt::deserialize(*custom);
            }
            return banner;
        }
    }
    return std::nullopt;
}

/// parity: `Player.drop(stack, false)`, the toss a full inventory falls back on.
void tossFrom(const PlayerSession& player, world::LoadedWorld& world, std::int32_t entityId,
              const storage::StoredItemStack& stack, std::int64_t seed) {
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
        wire::common::Identifier::parse(stack.id);
    if (!id.has_value()) {
        return;
    }
    constexpr auto kRadians = static_cast<float>(std::numbers::pi / 180.0);
    random::RandomSource random = random::RandomSource::legacy(seed);
    const float sinX = wire::common::mth::sin(player.pitch * kRadians);
    const float cosX = wire::common::mth::cos(player.pitch * kRadians);
    const float sinY = wire::common::mth::sin(player.yaw * kRadians);
    const float cosY = wire::common::mth::cos(player.yaw * kRadians);
    const float angle = random.nextFloat() * (2.0F * std::numbers::pi_v<float>);
    const float spread = 0.02F * random.nextFloat();
    const float up = (-sinX * 0.3F) + 0.1F + ((random.nextFloat() - random.nextFloat()) * 0.1F);
    world::DroppedItem dropped{
        .entityId = entityId,
        .uuid = wire::common::Uuid::random(),
        .item = *id,
        .count = stack.count,
        .x = player.x,
        .y = player.y + world::fluids::eyeHeight(player.sneaking) - static_cast<double>(0.3F),
        .z = player.z,
        .velocityX = static_cast<double>((-sinY * cosX * 0.3F) + (wire::common::mth::cos(angle) * spread)),
        .velocityY = static_cast<double>(up),
        .velocityZ = static_cast<double>((cosY * cosX * 0.3F) + (wire::common::mth::sin(angle) * spread))};
    dropped.components = stack.components;
    static_cast<void>(world.state().spawnItem(std::move(dropped)));
}

} // namespace

void sendMapUpdate(const PlayerSession& player, const play::MapItemDataPacket& update) {
    if (player.connection().sendMapData) {
        player.connection().sendMapData(update);
    } else if (player.connection().sendPlayPacket) {
        player.connection().sendPlayPacket(play::MapItemDataPacket::kId,
                                           [&update](wire::io::ByteBuffer& buffer) { update.write(buffer); });
    }
}

MapSystem::MapSystem(player::PlayerRegistry& players, maps::MapStorage& storage, Options options)
    : players_(&players), storage_(&storage), options_(std::move(options)) {}

bool MapSystem::isMap(std::string_view item) { return std::ranges::find(kMapItems, item) != kMapItems.end(); }

std::optional<std::int32_t> MapSystem::mapIdOf(const NbtCompound& components) {
    const wire::nbt::NbtTag* id = components.find(kMapId);
    if (id == nullptr) {
        return std::nullopt;
    }
    if (const std::int32_t* value = id->get<std::int32_t>(); value != nullptr) {
        return *value;
    }
    return std::nullopt;
}

bool MapSystem::carries(const PlayerSession& player, std::string_view item, std::int32_t mapId) const {
    for (std::int32_t slot = 0; slot < storage::StoredInventory::kPlayerSlots; ++slot) {
        const std::optional<storage::StoredItemStack>& stack = player.inventory.at(slot);
        if (stack.has_value() && stack->id == item && mapIdOf(stack->components) == mapId) {
            return true;
        }
    }
    return false;
}

bool MapSystem::hidden(const PlayerSession& player) const {
    if (options_.itemTags == nullptr) {
        return false;
    }
    for (std::int32_t slot = PlayerInventory::kArmourStart;
         slot < PlayerInventory::kArmourStart + PlayerInventory::kArmourSlots; ++slot) {
        const std::optional<storage::StoredItemStack>& stack = player.inventory.at(slot);
        if (!stack.has_value()) {
            continue;
        }
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            wire::common::Identifier::parse(stack->id);
        if (id.has_value() && world::realItemTags::contains(*options_.itemTags, kInvisibilityTag, *id)) {
            return true;
        }
    }
    return false;
}

void MapSystem::tickCarriedBy(maps::MapData& data, PlayerSession& ticking, const storage::StoredItemStack& stack,
                              const FrameView* frame, std::int64_t gameTime) {
    static_cast<void>(data.holder(ticking.uuid(), ticking.entityId(), ticking.name()));
    const std::int32_t mapId = mapIdOf(stack.components).value_or(-1);
    if (!carries(ticking, stack.id, mapId)) {
        data.removeDecoration(ticking.name());
    }
    std::vector<maps::MapData::Holder>& holders = data.holders();
    for (std::size_t index = 0; index < holders.size(); ++index) {
        const std::string name = holders[index].name;
        PlayerSession* other = players_->get(holders[index].player);
        if (other != nullptr && other->entityId() != holders[index].entityId) {
            other = nullptr;
        }
        if (other != nullptr && (frame != nullptr || carries(*other, stack.id, mapId))) {
            world::LoadedWorld* otherWorld = options_.worldNamed ? options_.worldNamed(other->worldName) : nullptr;
            if (frame == nullptr && otherWorld != nullptr && otherWorld->levelKey() == data.dimension() &&
                data.trackingPosition()) {
                data.addDecoration(maps::decorationTypes::kPlayer, otherWorld->gameTime(), name, other->x, other->z,
                                   other->yaw, std::nullopt);
            }
        } else {
            // parity: the jar removes from the list it is walking and still steps on, so the holder
            // after a removed one waits for the next tick.
            holders.erase(holders.begin() + static_cast<std::ptrdiff_t>(index));
            data.removeDecoration(name);
        }
        if (other != nullptr && other != &ticking && hidden(*other)) {
            data.removeDecoration(name);
        }
    }
    if (frame != nullptr && data.trackingPosition()) {
        data.trackFrame(frame->frame, gameTime);
    }
    if (const wire::nbt::NbtTag* decorations = stack.components.find(kMapDecorations); decorations != nullptr) {
        if (const NbtCompound* entries = decorations->get<NbtCompound>(); entries != nullptr) {
            data.addStaticDecorations(*entries, gameTime);
        }
    }
}

void MapSystem::update(world::LoadedWorld& world, maps::MapData& data, PlayerSession& player) {
    if (world.levelKey() != data.dimension()) {
        return;
    }
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    const maps::ScanWorld scan{
        .chunkAt = [&world](std::int32_t chunkX, std::int32_t chunkZ) -> const server::world::Chunk* {
            return world.state().chunkIfLoaded(chunkX, chunkZ);
        },
        .bannerAt = [&world](const maps::BlockPos& pos) { return bannerAt(world, pos); },
        .minY = facts.chunkHeight.minY(),
        .hasCeiling = facts.hasCeiling};
    maps::MapData::Holder& holder = data.holder(player.uuid(), player.entityId(), player.name());
    maps::mapRenderer::update(data, holder, scan, player.x, player.z);
}

void MapSystem::tickPlayers(world::LoadedWorld& world) {
    const std::unique_lock<std::mutex> lock = storage_->lock();
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName != world.name()) {
            continue;
        }
        const std::int32_t held = PlayerInventory::kHotbarStart + player->inventory.heldSlot();
        const auto tickSlot = [&](std::int32_t slot, bool hand) {
            const std::optional<storage::StoredItemStack>& stack = player->inventory.at(slot);
            if (!stack.has_value() || !isMap(stack->id)) {
                return;
            }
            const std::optional<std::int32_t> id = mapIdOf(stack->components);
            maps::MapData* data = id.has_value() ? storage_->get(*id) : nullptr;
            if (data == nullptr) {
                return;
            }
            tickCarriedBy(*data, *player, *stack, nullptr, world.gameTime());
            if (!data->isLocked() && hand) {
                update(world, *data, *player);
            }
        };
        for (std::int32_t index = 0; index < PlayerInventory::kMainSlots; ++index) {
            const std::int32_t slot = vanillaMainSlot(index);
            tickSlot(slot, slot == held);
        }
        for (const std::int32_t slot : kEquipmentTickOrder) {
            tickSlot(slot, slot == PlayerInventory::kOffhandSlot);
        }
        const auto sync = [&](std::int32_t slot) {
            const std::optional<storage::StoredItemStack>& stack = player->inventory.at(slot);
            const std::optional<std::int32_t> id = stack.has_value() ? mapIdOf(stack->components) : std::nullopt;
            maps::MapData* data = id.has_value() ? storage_->get(*id) : nullptr;
            maps::MapData::Holder* holder = data == nullptr ? nullptr : data->findHolder(player->uuid());
            if (holder == nullptr) {
                return;
            }
            if (const std::optional<play::MapItemDataPacket> packet = data->nextUpdate(*id, *holder);
                packet.has_value()) {
                sendMapUpdate(*player, *packet);
            }
        };
        for (std::int32_t index = 0; index < PlayerInventory::kMainSlots; ++index) {
            sync(vanillaMainSlot(index));
        }
        for (const std::int32_t slot : kEquipmentSyncOrder) {
            sync(slot);
        }
    }
}

void MapSystem::tickFrames(world::LoadedWorld& world) {
    if (world.gameTime() % kFrameInterval != 0) {
        return;
    }
    std::vector<entity::ServerEntity*> frames;
    for (entity::ServerEntity* entity : world.state().entities()) {
        if (entity != nullptr && !entity->removed && world::itemFrames::isFrame(entity->type())) {
            frames.push_back(entity);
        }
    }
    const std::unique_lock<std::mutex> lock = storage_->lock();
    sweepFrames(world, frames);
    std::vector<PlayerSession*> watchers;
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }
    for (entity::ServerEntity* frame : frames) {
        const std::optional<server::world::loot::ItemStack> shown = world::itemFrames::shownIn(*frame);
        if (!shown.has_value() || !isMap(shown->item.toString())) {
            continue;
        }
        const std::optional<std::int32_t> id = mapIdOf(shown->components);
        maps::MapData* data = id.has_value() ? storage_->get(*id) : nullptr;
        if (data == nullptr) {
            continue;
        }
        const storage::StoredItemStack stack{.id = shown->item.toString(), .count = 1, .components = shown->components};
        const std::int32_t facing = frame->sourceNbt.has_value() ? frame->sourceNbt->getByte("Facing", 0) : 0;
        const FrameView view{.frame = {.pos = world::itemFrames::squareOf(*frame),
                                       .rotation = twoDimensionalValue(facing) * 90,
                                       .entityId = frame->entityId()}};
        for (PlayerSession* player : watchers) {
            tickCarriedBy(*data, *player, stack, &view, world.gameTime());
            if (maps::MapData::Holder* holder = data->findHolder(player->uuid()); holder != nullptr) {
                if (const std::optional<play::MapItemDataPacket> packet = data->nextUpdate(*id, *holder);
                    packet.has_value()) {
                    sendMapUpdate(*player, *packet);
                }
            }
        }
    }
}

void MapSystem::sweepFrames(world::LoadedWorld& world, const std::vector<entity::ServerEntity*>& frames) {
    // The jar clears a frame's marker the moment its map leaves the frame. Here a marker whose frame
    // stands in a loaded chunk but no longer shows that map is cleared on the next framed-map pass.
    std::vector<std::pair<maps::BlockPos, std::int32_t>> gone;
    storage_->forEachLoaded([&](std::int32_t id, maps::MapData& data) {
        if (data.frames().empty() || data.dimension() != world.levelKey()) {
            return;
        }
        gone.clear();
        for (const auto& [key, marker] : data.frames()) {
            if (!world.state().isLoaded(marker.pos[0] >> 4, marker.pos[2] >> 4)) {
                continue;
            }
            const bool shown = std::ranges::any_of(frames, [&](const entity::ServerEntity* frame) {
                if (frame->entityId() != marker.entityId) {
                    return false;
                }
                const std::optional<server::world::loot::ItemStack> held = world::itemFrames::shownIn(*frame);
                return held.has_value() && mapIdOf(held->components) == id;
            });
            if (!shown) {
                gone.emplace_back(marker.pos, marker.entityId);
            }
        }
        for (const auto& [pos, entityId] : gone) {
            data.removedFromFrame(pos, entityId);
        }
    });
}

bool MapSystem::useEmptyMap(const player::BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                            std::int32_t slot) {
    const std::optional<storage::StoredItemStack> held = player.inventory.at(slot);
    if (!held.has_value() || held->id != kEmptyMap) {
        return false;
    }
    storage::StoredItemStack map{.id = std::string{kFilledMap}, .count = 1};
    {
        const std::unique_lock<std::mutex> lock = storage_->lock();
        const std::int32_t id = storage_->nextId();
        storage_->set(id,
                      maps::MapData::createFresh(wire::common::mth::floor(player.x), wire::common::mth::floor(player.z),
                                                 0, true, false, world.levelKey()));
        map.components.putInt(kMapId, id);
    }
    // parity: `ItemStack.consume`, which a creative player does not pay.
    std::optional<storage::StoredItemStack> rest = held;
    if (player.gamemode != play::Gamemode::Creative) {
        --rest->count;
    }
    const std::int64_t seed = options_.nextSeed ? options_.nextSeed() : 0;
    for (PlayerSession* listener : players_->snapshot()) {
        if (listener->worldName == world.name()) {
            player::playSoundFor(*listener, kTakeResultSound, play::SoundSource::Players, player.x, player.y, player.z,
                                 1.0F, 1.0F, seed);
        }
    }
    if (rest->count <= 0) {
        player.inventory.set(slot, map);
    } else {
        player.inventory.set(slot, rest);
        if (player.inventory.add(map) > 0) {
            tossFrom(player, world, players_->allocateEntityId(), map, seed);
        }
    }
    if (context.slotChanged) {
        context.slotChanged(player, slot);
    }
    return true;
}

std::optional<bool> MapSystem::useOnBanner(PlayerSession& player, world::LoadedWorld& world, std::int32_t slot,
                                           std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<storage::StoredItemStack>& held = player.inventory.at(slot);
    if (!held.has_value() || !isMap(held->id)) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    const wire::common::Identifier* block =
        state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
    if (block == nullptr || !block->path().ends_with("_banner")) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> id = mapIdOf(held->components);
    const std::unique_lock<std::mutex> lock = storage_->lock();
    maps::MapData* data = id.has_value() ? storage_->get(*id) : nullptr;
    if (data == nullptr) {
        return true;
    }
    const maps::BlockPos pos{x, y, z};
    return data->toggleBanner(bannerAt(world, pos), pos, world.gameTime());
}

void MapSystem::postProcessCrafted(NbtCompound& components, const world::LoadedWorld& world) {
    const wire::nbt::NbtTag* tag = components.find(kPostProcessing);
    if (tag == nullptr) {
        return;
    }
    const std::string kind = tag->get<std::string>() == nullptr ? std::string{} : *tag->get<std::string>();
    components.remove(kPostProcessing);
    static_cast<void>(world);
    const std::optional<std::int32_t> id = mapIdOf(components);
    const std::unique_lock<std::mutex> lock = storage_->lock();
    maps::MapData* original = id.has_value() ? storage_->get(*id) : nullptr;
    if (original == nullptr) {
        return;
    }
    std::optional<maps::MapData> made;
    if (kind == "lock") {
        made = original->locked();
    } else if (kind == "scale") {
        made = original->scaled();
    }
    if (!made.has_value()) {
        return;
    }
    const std::int32_t next = storage_->nextId();
    storage_->set(next, std::move(*made));
    components.putInt(kMapId, next);
}

void MapSystem::makeExplorerMapLocked(NbtCompound& components, const world::LoadedWorld& world, std::int32_t targetX,
                                      std::int32_t targetZ, std::int8_t zoom, std::string_view decoration,
                                      const std::function<bool(std::int32_t, std::int32_t, std::int32_t)>& watery,
                                      std::int32_t seaLevel) {
    const std::int32_t id = storage_->nextId();
    maps::MapData data = maps::MapData::createFresh(targetX, targetZ, zoom, true, true, world.levelKey());
    if (watery) {
        maps::mapRenderer::renderBiomePreview(data, seaLevel, watery);
    }
    storage_->set(id, std::move(data));
    components.putInt(kMapId, id);
    // parity: `addTargetDecoration`, the "+" entry pointing at the structure.
    NbtCompound entry;
    std::string type{decoration};
    if (!type.contains(':')) {
        type = "minecraft:" + type;
    }
    entry.putString("type", std::move(type));
    entry.putDouble("x", targetX);
    entry.putDouble("z", targetZ);
    entry.putFloat("rotation", 180.0F);
    NbtCompound decorations;
    if (const wire::nbt::NbtTag* existing = components.find(kMapDecorations); existing != nullptr) {
        if (const NbtCompound* old = existing->get<NbtCompound>(); old != nullptr) {
            decorations = *old;
        }
    }
    decorations.set("+", wire::nbt::NbtTag::ofCompound(std::move(entry)));
    components.set(kMapDecorations, wire::nbt::NbtTag::ofCompound(std::move(decorations)));
}

void MapSystem::sendWhole(const PlayerSession& player, std::int32_t mapId) {
    const std::unique_lock<std::mutex> lock = storage_->lock();
    maps::MapData* data = storage_->get(mapId);
    if (data == nullptr) {
        return;
    }
    maps::MapData::Holder whole{.player = player.uuid(), .entityId = player.entityId(), .name = player.name()};
    if (std::optional<play::MapItemDataPacket> packet = data->nextUpdate(mapId, whole); packet.has_value()) {
        sendMapUpdate(player, *packet);
    }
}

std::optional<MapSystem::Summary> MapSystem::summaryOf(std::int32_t mapId) {
    const std::unique_lock<std::mutex> lock = storage_->lock();
    const maps::MapData* data = storage_->get(mapId);
    return data == nullptr ? std::nullopt : std::optional{Summary{.scale = data->scale(), .locked = data->isLocked()}};
}

void MapSystem::save() {
    const std::unique_lock<std::mutex> lock = storage_->lock();
    storage_->save();
}

} // namespace basalt::server::runtime
