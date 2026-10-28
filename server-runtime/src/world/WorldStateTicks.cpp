#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/FallingBlocks.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/NaturalSpawnObstruction.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/NoiseWorldGenerator.hpp>
#include <basalt/server/world/terrain/decoration/BushSurvival.hpp>
#include <basalt/server/world/terrain/decoration/tree/LeafDistances.hpp>
#include <basalt/wire/io/Platform.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <span>
#include <utility>

namespace basalt::server::runtime::world {
using server::world::Chunk;
using server::world::ChunkScheduledTick;

std::uint64_t WorldState::shapeChangesSince(std::uint64_t seen, std::vector<std::array<std::int32_t, 3>>& changed,
                                            bool& overflowed) const {
    const WorldReadLock lock{mutex_};
    overflowed = shapeChangeCount_ - seen > kShapeChangeMemory;
    for (std::uint64_t at = overflowed ? shapeChangeCount_ - kShapeChangeMemory : seen; at < shapeChangeCount_; ++at) {
        changed.push_back(shapeChanges_[at % kShapeChangeMemory]);
    }
    return shapeChangeCount_;
}

bool WorldState::hasSculkSensorNear(double x, double y, double z, std::int32_t range) const {
    return sculkSensors_.hasNear(x, y, z, range);
}

void WorldState::scheduleFluidTick(std::int32_t x, std::int32_t y, std::int32_t z, const wire::common::Identifier& type,
                                   std::int32_t delay) {
    mine("A scheduled fluid tick");
    const WorldWriteLock lock{mutex_};
    Chunk* chunk = chunkAtLocked(key(x >> 4, z >> 4));
    if (chunk == nullptr || y < chunk->verticalBounds().minY() || y > chunk->verticalBounds().maxY()) {
        return;
    }
    chunk->scheduleFluidTick(
        ChunkScheduledTick{.type = type, .localX = x & 15, .y = y, .localZ = z & 15, .delay = delay});
}

void WorldState::scheduleBlockTick(std::int32_t x, std::int32_t y, std::int32_t z, const wire::common::Identifier& type,
                                   std::int32_t delay, std::int32_t priority) {
    mine("A scheduled tick");
    const WorldWriteLock lock{mutex_};
    Chunk* chunk = chunkAtLocked(key(x >> 4, z >> 4));
    if (chunk == nullptr || y < chunk->verticalBounds().minY() || y > chunk->verticalBounds().maxY()) {
        return;
    }
    chunk->scheduleBlockTick(ChunkScheduledTick{.type = type,
                                                .localX = x & 15,
                                                .y = y,
                                                .localZ = z & 15,
                                                .delay = delay,
                                                .priority = priority,
                                                .order = nextTickOrder_++});
}

bool WorldState::hasScheduledBlockTick(std::int32_t x, std::int32_t y, std::int32_t z,
                                       const wire::common::Identifier& type) const {
    const WorldReadLock lock{mutex_};
    const Chunk* chunk = chunkAtLocked(key(x >> 4, z >> 4));
    if (chunk == nullptr) {
        return false;
    }
    for (const ChunkScheduledTick& tick : chunk->scheduledBlockTicks()) {
        if (tick.type == type && tick.localX == (x & 15) && tick.y == y && tick.localZ == (z & 15)) {
            return true;
        }
    }
    return false;
}

std::vector<std::array<std::int32_t, 3>>
WorldState::dueTicks(const std::function<void(Chunk&, std::vector<ChunkScheduledTick>&)>& advance, bool remember,
                     const Ticking& ticking) {
    std::vector<ChunkScheduledTick> due;
    std::vector<std::array<std::int32_t, 4>> found;
    const WorldWriteLock lock{mutex_};
    for (const auto& [chunkKey, chunk] : chunksByPos_) {
        if (chunk->scheduledTickCount() == 0) {
            continue;
        }
        // parity: a chunk that is loaded and not ticking is frozen: `ChunkMap` takes its.
        if (ticking && !ticking(chunk->chunkX(), chunk->chunkZ())) {
            continue;
        }
        const std::size_t before = due.size();
        advance(*chunk, due);
        for (std::size_t index = before; index < due.size(); ++index) {
            const ChunkScheduledTick& tick = due[index];
            found.push_back(std::array<std::int32_t, 4>{(chunk->chunkX() << 4) + tick.localX, tick.y,
                                                        (chunk->chunkZ() << 4) + tick.localZ,
                                                        static_cast<std::int32_t>(index)});
        }
    }
    // parity: `ScheduledTick.INTRA_TICK_DRAIN_ORDER`.
    std::ranges::stable_sort(
        found, [&due](const std::array<std::int32_t, 4>& left, const std::array<std::int32_t, 4>& right) {
            const ChunkScheduledTick& one = due[static_cast<std::size_t>(left[3])];
            const ChunkScheduledTick& other = due[static_cast<std::size_t>(right[3])];
            return one.priority != other.priority ? one.priority < other.priority : one.order < other.order;
        });
    if (remember) {
        collectedBlockTicks_.clear();
        collectedBlockTicks_.reserve(found.size());
    }
    std::vector<std::array<std::int32_t, 3>> positions;
    positions.reserve(found.size());
    for (const std::array<std::int32_t, 4>& one : found) {
        positions.push_back(std::array<std::int32_t, 3>{one[0], one[1], one[2]});
        if (remember) {
            collectedBlockTicks_.push_back(CollectedTick{
                .x = one[0], .y = one[1], .z = one[2], .type = due[static_cast<std::size_t>(one[3])].type});
        }
    }
    return positions;
}

void WorldState::openTurn(const Ticking& ticking) {
    const WorldWriteLock lock{mutex_};
    for (const auto& [chunkKey, chunk] : chunksByPos_) {
        if (chunk->scheduledTickCount() == 0 || (ticking && !ticking(chunk->chunkX(), chunk->chunkZ()))) {
            continue;
        }
        chunk->openTurn();
    }
}

std::vector<std::array<std::int32_t, 3>> WorldState::dueFluidTicks(const Ticking& ticking) {
    return dueTicks([](Chunk& chunk, std::vector<ChunkScheduledTick>& due) { chunk.advanceFluidTicks(due); }, false,
                    ticking);
}

std::vector<std::array<std::int32_t, 3>> WorldState::dueBlockTicks(const Ticking& ticking) {
    std::vector<std::array<std::int32_t, 3>> positions;
    for (const DueTick& tick : dueBlockTicksTyped(ticking)) {
        positions.push_back(std::array<std::int32_t, 3>{tick.x, tick.y, tick.z});
    }
    return positions;
}

std::vector<WorldState::DueTick> WorldState::dueBlockTicksTyped(const Ticking& ticking) {
    const std::vector<std::array<std::int32_t, 3>> positions = dueTicks(
        [](Chunk& chunk, std::vector<ChunkScheduledTick>& due) { chunk.advanceBlockTicks(due); }, true, ticking);
    const WorldReadLock lock{mutex_};
    std::vector<DueTick> due;
    due.reserve(positions.size());
    for (std::size_t index = 0; index < positions.size() && index < collectedBlockTicks_.size(); ++index) {
        due.push_back(DueTick{.x = positions[index][0],
                              .y = positions[index][1],
                              .z = positions[index][2],
                              .type = collectedBlockTicks_[index].type});
    }
    return due;
}

bool WorldState::willTickThisTick(std::int32_t x, std::int32_t y, std::int32_t z,
                                  const wire::common::Identifier& type) const {
    const WorldReadLock lock{mutex_};
    return std::ranges::any_of(collectedBlockTicks_, [&](const CollectedTick& one) {
        return one.x == x && one.y == y && one.z == z && one.type == type;
    });
}

void WorldState::noteSlideStarted(std::int32_t x, std::int32_t y, std::int32_t z) {
    const WorldWriteLock lock{mutex_};
    slidesStarted_.push_back(std::array<std::int32_t, 3>{x, y, z});
}

bool WorldState::slideStartedThisTick(std::int32_t x, std::int32_t y, std::int32_t z) const {
    const WorldReadLock lock{mutex_};
    return std::ranges::any_of(slidesStarted_, [x, y, z](const std::array<std::int32_t, 3>& at) {
        return at[0] == x && at[1] == y && at[2] == z;
    });
}

void WorldState::clearSlidesStarted() {
    const WorldWriteLock lock{mutex_};
    slidesStarted_.clear();
}

void WorldState::endBlockTicks() {
    const WorldWriteLock lock{mutex_};
    collectedBlockTicks_.clear();
}

void WorldState::beginBlockTick(std::int32_t x, std::int32_t y, std::int32_t z) {
    const WorldWriteLock lock{mutex_};
    std::erase_if(collectedBlockTicks_,
                  [&](const CollectedTick& one) { return one.x == x && one.y == y && one.z == z; });
}

std::vector<GeneratedWorldEntity> WorldState::generatedEntities(const std::function<std::int32_t()>& allocateEntityId,
                                                                std::int32_t centerChunkX, std::int32_t centerChunkZ,
                                                                std::int32_t radiusChunks) {
    return pendingDecorations_.generatedEntities(allocateEntityId, centerChunkX, centerChunkZ, radiusChunks,
                                                 [this](std::int64_t chunkKey) { return chunkAtLocked(chunkKey); });
}

DroppedItem& WorldState::spawnItem(DroppedItem dropped) {
    mine("A dropped item");
    const bool allowed = events().allowed(&server::plugins::pluginEventTypes::itemSpawn, [&](auto& event) {
        plugins::hookEvents::position(event, eventWorld_, static_cast<std::int32_t>(std::floor(dropped.x)),
                                      static_cast<std::int32_t>(std::floor(dropped.y)),
                                      static_cast<std::int32_t>(std::floor(dropped.z)));
        event.set("entity_id", static_cast<std::int64_t>(dropped.entityId));
        event.set("item", dropped.item.toString());
        event.set("count", static_cast<std::int64_t>(dropped.count));
        event.set("cause", std::string{dropped.thrower.has_value() ? "drop" : "spawn"});
    });
    DroppedItem& spawned = itemPhysics_.spawnItem(std::move(dropped));
    if (!allowed) {
        spawned.health = 0;
    }
    return spawned;
}

std::vector<DroppedItem> WorldState::items() const { return itemPhysics_.items(); }

std::optional<DroppedItem> WorldState::itemById(std::int32_t entityId) const { return itemPhysics_.itemById(entityId); }

std::vector<DroppedItem> WorldState::nearbyItems(double x, double y, double z, double radius) const {
    return itemPhysics_.nearbyItems(x, y, z, radius);
}

bool WorldState::shiftItem(std::int32_t entityId, double dx, double dy, double dz) {
    return itemPhysics_.shiftItem(entityId, dx, dy, dz);
}

std::vector<DroppedItem> WorldState::takeDestroyedItems() { return itemPhysics_.takeDestroyed(); }

bool WorldState::hurtItem(const std::int32_t entityId, const std::int32_t amount) {
    return itemPhysics_.hurtItem(entityId, amount);
}

bool WorldState::pushItem(const std::int32_t entityId, const double dx, const double dy, const double dz) {
    return itemPhysics_.pushItem(entityId, dx, dy, dz);
}

bool WorldState::setItemCount(const std::int32_t entityId, const std::int32_t count) {
    mine("A dropped item");
    return itemPhysics_.setItemCount(entityId, count);
}

std::optional<DroppedItem> WorldState::removeItem(std::int32_t entityId) {
    mine("A dropped item");
    return itemPhysics_.removeItem(entityId);
}

void WorldState::setEvents(const plugins::EventSink* events, std::string worldName) {
    events_ = events;
    eventWorld_ = std::move(worldName);
    const auto describe = [this](server::plugins::PluginEvent& event, const DroppedItem& item) {
        plugins::hookEvents::position(event, eventWorld_, static_cast<std::int32_t>(std::floor(item.x)),
                                      static_cast<std::int32_t>(std::floor(item.y)),
                                      static_cast<std::int32_t>(std::floor(item.z)));
        event.set("entity_id", static_cast<std::int64_t>(item.entityId));
        event.set("item", item.item.toString());
        event.set("count", static_cast<std::int64_t>(item.count));
    };
    itemPhysics_.setMergeGate(
        [this, describe](const DroppedItem& into, const DroppedItem& from) {
            return this->events().allowed(&server::plugins::pluginEventTypes::itemMerge,
                                          [&](server::plugins::PluginEvent& event) {
                                              describe(event, into);
                                              event.set("source_id", static_cast<std::int64_t>(from.entityId));
                                              event.set("source_count", static_cast<std::int64_t>(from.count));
                                          });
        },
        [this] { return this->events().listening(&server::plugins::pluginEventTypes::itemMerge); });
}

std::vector<std::int32_t> WorldState::tickItems(const sched::Region* region) {
    if (events().listening(&server::plugins::pluginEventTypes::itemDespawn)) {
        for (const DroppedItem& item : itemPhysics_.expiring(region)) {
            if (!events().allowed(
                    &server::plugins::pluginEventTypes::itemDespawn, [&](server::plugins::PluginEvent& event) {
                        plugins::hookEvents::position(event, eventWorld_, static_cast<std::int32_t>(std::floor(item.x)),
                                                      static_cast<std::int32_t>(std::floor(item.y)),
                                                      static_cast<std::int32_t>(std::floor(item.z)));
                        event.set("entity_id", static_cast<std::int64_t>(item.entityId));
                        event.set("item", item.item.toString());
                        event.set("count", static_cast<std::int64_t>(item.count));
                    })) {
                itemPhysics_.restartAge(item.entityId);
            }
        }
    }
    return itemPhysics_.tickItems(region);
}

std::vector<entity::ServerEntity*> WorldState::entities() const { return entityRegistry_.entities(); }

bool WorldState::hasNaturalSpawnEntityObstruction(const entity::Aabb& spawnBox, const entity::Aabb& entityBox) const {
    return entityRegistry_.hasNaturalSpawnObstruction(spawnBox, entityBox);
}

std::vector<entity::ServerEntity*> WorldState::entitiesIn(std::int32_t chunkX, std::int32_t chunkZ) const {
    return entityRegistry_.entitiesIn(chunkX, chunkZ);
}

std::vector<std::pair<std::int32_t, std::int32_t>> WorldState::chunksWithEntities() const {
    std::vector<std::pair<std::int32_t, std::int32_t>> chunks = entityRegistry_.chunksWithEntities();
    for (const DroppedItem& item : itemPhysics_.items()) {
        const std::pair<std::int32_t, std::int32_t> at{static_cast<std::int32_t>(std::floor(item.x)) >> 4,
                                                       static_cast<std::int32_t>(std::floor(item.z)) >> 4};
        if (std::ranges::find(chunks, at) == chunks.end()) {
            chunks.push_back(at);
        }
    }
    return chunks;
}

void WorldState::forgetEntitiesIn(std::int32_t chunkX, std::int32_t chunkZ) {
    entityRegistry_.forgetEntitiesIn(chunkX, chunkZ);
}

void WorldState::addPendingEntities(std::vector<wire::nbt::NbtCompound> saved) {
    entityRegistry_.addPendingEntities(std::move(saved));
}

std::vector<wire::nbt::NbtCompound> WorldState::takePendingEntities() { return entityRegistry_.takePendingEntities(); }

std::vector<server::world::ChunkGeneratedEntity> WorldState::takePendingGeneratedEntities() {
    return pendingDecorations_.takePendingGeneratedEntities();
}

entity::ServerEntity* WorldState::entity(std::int32_t entityId) const { return entityRegistry_.entity(entityId); }

entity::ServerEntity& WorldState::spawnEntity(std::unique_ptr<entity::ServerEntity> entity) {
    mine("An entity");
    return entityRegistry_.spawnEntity(std::move(entity));
}

void WorldState::setMobSettings(const config::MobSettings* settings) {
    mobSettings_ = settings;
    entityRegistry_.setMobSettings(settings);
    if (settings != nullptr) {
        itemPhysics_.setRules(settings->itemDespawnTicks, settings->itemMergeRadius);
    }
}

const config::MobSettings& WorldState::mobSettings() const noexcept {
    static const config::MobSettings kVanilla;
    return mobSettings_ != nullptr ? *mobSettings_ : kVanilla;
}

std::vector<std::int32_t> WorldState::collectRemovedEntities() { return entityRegistry_.collectRemovedEntities(); }

bool WorldState::tntMayPrime(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view reason,
                             std::string_view who) const {
    return events().allowed(&server::plugins::pluginEventTypes::blockTntPrime,
                            [&](server::plugins::PluginEvent& event) {
                                plugins::hookEvents::position(event, eventWorld_, x, y, z);
                                event.set("reason", std::string{reason});
                                event.set("player_name", std::string{who});
                            });
}

void WorldState::primeTnt(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view event) {
    primedTntQueue_.primeTnt(x, y, z, event);
}

std::vector<PrimedTntQueue::Primed> WorldState::takePrimedTnt() { return primedTntQueue_.takePrimedTnt(); }

std::vector<WorldState::Spilled> WorldState::takeSpilledContents() {
    const WorldWriteLock lock{mutex_};
    return std::exchange(spilled_, {});
}

void WorldState::queueStructureTrigger(std::int32_t x, std::int32_t y, std::int32_t z) {
    const WorldWriteLock lock{mutex_};
    structureTriggers_.push_back({x, y, z});
}

void WorldState::setTestTriggered(std::int32_t x, std::int32_t y, std::int32_t z, bool triggered) {
    const WorldWriteLock lock{mutex_};
    if (triggered) {
        triggeredTestBlocks_.insert({x, y, z});
    } else {
        triggeredTestBlocks_.erase({x, y, z});
    }
}

bool WorldState::testTriggered(std::int32_t x, std::int32_t y, std::int32_t z) const {
    const WorldReadLock lock{mutex_};
    return triggeredTestBlocks_.contains({x, y, z});
}

std::vector<std::array<std::int32_t, 3>> WorldState::takeStructureTriggers() {
    const WorldWriteLock lock{mutex_};
    return std::exchange(structureTriggers_, {});
}

} // namespace basalt::server::runtime::world
