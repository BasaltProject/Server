#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/NaturalSpawnObstruction.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/NoiseWorldGenerator.hpp>
#include <basalt/server/world/terrain/decoration/BushSurvival.hpp>
#include <basalt/server/world/terrain/decoration/tree/LeafDistances.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <utility>

namespace basalt::server::runtime::world {

using server::world::Chunk;
using server::world::ChunkScheduledTick;

namespace {

/// A generated entity that acts, rather than one the chunk only draws: a mob, or a pillar's
/// crystal, which burns, explodes and heals the dragon.
[[nodiscard]] bool becomesLive(const server::world::ChunkGeneratedEntity& entity) {
    return entity::mobs::isMob(entity.type) || endCrystals::isCrystal(entity.type);
}

} // namespace

WorldState::WorldState(std::vector<Chunk> initialChunks, LoadOrGenerate loadOrGenerate)
    : loadOrGenerate_(
          loadOrGenerate
              ? std::move(loadOrGenerate)
              : [](std::int32_t chunkX,
                   std::int32_t chunkZ) { return server::world::noiseWorldGenerator::generate(chunkX, chunkZ); }),
      primedTntQueue_(mutex_), pendingDecorations_(mutex_), sculkSensors_(mutex_), entityRegistry_(mutex_),
      itemPhysics_(mutex_, lockedBlockAccessor()) {
    // Tests and embedders may seed a world with detached generated chunks. Production starts empty,
    // but those chunks still deserve the same cross-border handoff an install gives.
    for (Chunk& chunk : initialChunks) {
        const std::int64_t chunkKey = key(chunk.chunkX(), chunk.chunkZ());
        chunksByPos_.emplace(chunkKey, std::make_unique<Chunk>(std::move(chunk)));
    }
    const PendingChunkDecorations::ChunkAt chunkAt = [this](std::int64_t chunkKey) { return chunkAtLocked(chunkKey); };
    for (const auto& [chunkKey, chunk] : chunksByPos_) {
        pendingDecorations_.addPendingGeneratedEntities(chunk->takeGeneratedEntitiesMatching(
            [](const server::world::ChunkGeneratedEntity& entity) { return becomesLive(entity); }));
        pendingDecorations_.applyPendingGeneratedBlockWrites(*chunk,
                                                             PendingChunkDecorations::WritePhase::AfterDecoration);
        pendingDecorations_.applyPendingGeneratedBlockEntityWrites(*chunk);
        pendingDecorations_.applyPendingGeneratedBlockTicks(*chunk);
        pendingDecorations_.deliverGeneratedBlockWrites(chunk->takeGeneratedBlockWrites(), chunk->decorationSequence(),
                                                        chunkAt);
        pendingDecorations_.deliverGeneratedBlockEntityWrites(chunk->takeGeneratedBlockEntityWrites(), chunkAt);
        pendingDecorations_.deliverGeneratedBlockTicks(chunk->takeGeneratedBlockTicks(), chunkAt);
    }
}

std::int64_t WorldState::key(std::int32_t chunkX, std::int32_t chunkZ) {
    return (static_cast<std::int64_t>(chunkX) & 0xFFFFFFFFLL) | (static_cast<std::int64_t>(chunkZ) << 32);
}

void WorldState::mine(std::string_view what) const {
    if (regionCount_ && regionCount_() > 1) {
        return;
    }
    if (owner_ != nullptr) {
        owner_->check(what);
    }
}

Chunk* WorldState::chunkAtLocked(std::int64_t chunkKey) const {
    const auto found = chunksByPos_.find(chunkKey);
    return found == chunksByPos_.end() ? nullptr : found->second.get();
}

std::vector<Chunk*> WorldState::snapshot() const {
    const WorldReadLock lock{mutex_};
    std::vector<Chunk*> every;
    every.reserve(chunksByPos_.size());
    for (const auto& [chunkKey, chunk] : chunksByPos_) {
        every.push_back(chunk.get());
    }
    return every;
}

std::vector<Chunk*> WorldState::chunks() const { return snapshot(); }

std::size_t WorldState::loadedChunkCount() const {
    const WorldReadLock lock{mutex_};
    return chunksByPos_.size();
}

WorldState::ChunkMemory WorldState::chunkMemory() const {
    const WorldReadLock lock{mutex_};
    ChunkMemory memory;
    for (const auto& [chunkKey, chunk] : chunksByPos_) {
        memory.bytes += chunk->residentBytes();
        memory.blockEntities += static_cast<std::int32_t>(chunk->blockEntityCount());
        memory.scheduledTicks += static_cast<std::int32_t>(chunk->scheduledTickCount());
        memory.generatedEntities += static_cast<std::int32_t>(chunk->generatedEntities().size());
    }
    return memory;
}

std::vector<Chunk*> WorldState::dirtyChunks() const {
    // Snapshotted under the lock and filtered outside it: every generation worker needs this same
    // lock twice per chunk, and holding it across a scan of everything loaded is how the thread
    // doing the scanning ends up never being scheduled at all.
    std::vector<Chunk*> dirty;
    for (Chunk* chunk : snapshot()) {
        if (chunk->isDirty()) {
            dirty.push_back(chunk);
        }
    }
    return dirty;
}

std::vector<Chunk*> WorldState::chunksWithStashedEntities() const {
    std::vector<Chunk*> stashed;
    for (Chunk* chunk : snapshot()) {
        if (!chunk->savedEntities().empty()) {
            stashed.push_back(chunk);
        }
    }
    return stashed;
}

std::vector<Chunk*>
WorldState::evictionCandidates(const std::function<bool(std::int32_t, std::int32_t)>& shouldKeep) const {
    std::vector<Chunk*> candidates;
    for (Chunk* chunk : snapshot()) {
        if (!shouldKeep(chunk->chunkX(), chunk->chunkZ())) {
            candidates.push_back(chunk);
        }
    }
    return candidates;
}

std::size_t WorldState::countMissing(const std::vector<std::pair<std::int32_t, std::int32_t>>& positions) const {
    const WorldReadLock lock{mutex_};
    std::size_t missing = 0;
    for (const auto& [chunkX, chunkZ] : positions) {
        if (!chunksByPos_.contains(key(chunkX, chunkZ))) {
            ++missing;
        }
    }
    return missing;
}

std::size_t WorldState::evictClean(const std::vector<Chunk*>& chunks) {
    const WorldWriteLock lock{mutex_};
    std::size_t removed = 0;
    for (Chunk* chunk : chunks) {
        if (chunk->isDirty()) {
            continue;
        }
        const std::int64_t chunkKey = key(chunk->chunkX(), chunk->chunkZ());
        const std::int32_t chunkX = chunk->chunkX();
        const std::int32_t chunkZ = chunk->chunkZ();
        if (chunksByPos_.erase(chunkKey) != 0) {
            pendingDecorations_.forgetGeneratedEntities(chunkKey);
            pendingDecorations_.forgetGeneratedWriteSources(chunkKey);
            sculkSensors_.forgetChunk(chunkX, chunkZ);
            ++removed;
        }
    }
    if (removed > 0) {
        chunkVersion_.fetch_add(1);
    }
    return removed;
}

std::vector<std::int64_t> WorldState::loadedChunkKeys() const {
    const WorldReadLock lock{mutex_};
    std::vector<std::int64_t> keys;
    keys.reserve(chunksByPos_.size());
    for (const auto& [chunkKey, chunk] : chunksByPos_) {
        keys.push_back(chunkKey);
    }
    return keys;
}

bool WorldState::isLoaded(std::int32_t chunkX, std::int32_t chunkZ) const {
    const WorldReadLock lock{mutex_};
    return chunksByPos_.contains(key(chunkX, chunkZ));
}

Chunk* WorldState::chunkIfLoaded(std::int32_t chunkX, std::int32_t chunkZ) const {
    const WorldReadLock lock{mutex_};
    return chunkAtLocked(key(chunkX, chunkZ));
}

std::optional<Chunk> WorldState::generateDetached(std::int32_t chunkX, std::int32_t chunkZ) {
    if (isLoaded(chunkX, chunkZ)) {
        return std::nullopt;
    }
    return loadOrGenerate_(chunkX, chunkZ);
}

Chunk& WorldState::getOrGenerateChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    if (Chunk* loaded = chunkIfLoaded(chunkX, chunkZ); loaded != nullptr) {
        return *loaded;
    }
    std::optional<Chunk> generated = generateDetached(chunkX, chunkZ);
    if (!generated.has_value()) {
        // It turned up while this was generating, so the one that arrived is the world's.
        return *chunkIfLoaded(chunkX, chunkZ);
    }
    return install(std::move(*generated));
}

std::optional<std::int32_t> WorldState::queuedWriteAt(const std::int32_t worldX, const std::int32_t y,
                                                      const std::int32_t worldZ) const {
    return pendingDecorations_.queuedWriteAt(worldX, y, worldZ);
}

void WorldState::queueBlockEvent(BlockEvent event) {
    const WorldWriteLock lock{mutex_};
    // parity: `ServerLevel.blockEvents` is a set, so the same event booked twice in one turn happens
    // once. A piston that hears two neighbours light up in the same tick moves once, not twice.
    const bool already = std::ranges::any_of(blockEvents_, [&event](const BlockEvent& queued) {
        return queued.x == event.x && queued.y == event.y && queued.z == event.z && queued.block == event.block &&
               queued.first == event.first && queued.second == event.second;
    });
    if (already) {
        return;
    }
    blockEvents_.push_back(std::move(event));
}

void WorldState::recordGameEvent(GameEventRecord record) {
    const WorldWriteLock lock{mutex_};
    std::erase_if(gameEvents_,
                  [&record](const GameEventRecord& kept) { return kept.gameTime < record.gameTime - 1; });
    gameEvents_.push_back(std::move(record));
}

std::vector<GameEventRecord> WorldState::recentGameEvents() const {
    const WorldReadLock lock{mutex_};
    return gameEvents_;
}

std::optional<BlockEvent> WorldState::takeFirstBlockEvent() {
    const WorldWriteLock lock{mutex_};
    if (blockEvents_.empty()) {
        return std::nullopt;
    }
    BlockEvent first = std::move(blockEvents_.front());
    blockEvents_.erase(blockEvents_.begin());
    return first;
}

std::vector<BlockEvent> WorldState::takeBlockEvents() {
    const WorldWriteLock lock{mutex_};
    return std::exchange(blockEvents_, {});
}

void WorldState::addTorchToggle(TorchToggle toggle) {
    const WorldWriteLock lock{mutex_};
    torchToggles_.push_back(toggle);
}

void WorldState::pruneTorchToggles(const std::int64_t now, const std::int64_t keepFor) {
    const WorldWriteLock lock{mutex_};
    while (!torchToggles_.empty() && now - torchToggles_.front().when > keepFor) {
        torchToggles_.pop_front();
    }
}

std::int32_t WorldState::torchToggleCount(const std::int32_t x, const std::int32_t y, const std::int32_t z) const {
    const WorldReadLock lock{mutex_};
    return static_cast<std::int32_t>(std::ranges::count_if(
        torchToggles_, [x, y, z](const TorchToggle& one) { return one.x == x && one.y == y && one.z == z; }));
}

void WorldState::applyPendingWritesTo(Chunk& chunk) {
    const WorldWriteLock lock{mutex_};
    pendingDecorations_.applyPendingGeneratedBlockWrites(chunk, PendingChunkDecorations::WritePhase::BeforeDecoration);
}

namespace {

/// North, south, west and east of a marked block, in the order the shape rules ask for them, or
/// nothing when one of them lives in a chunk that is not here yet.
[[nodiscard]] std::optional<std::array<std::int32_t, 4>>
horizontalNeighbours(Chunk& chunk, const server::world::ChunkShapeUpdate& mark,
                     const std::function<Chunk*(std::int32_t, std::int32_t)>& chunkAt) {
    static constexpr std::array<registry::BlockFace, 4> kSides{registry::BlockFace::North, registry::BlockFace::South,
                                                               registry::BlockFace::West, registry::BlockFace::East};
    std::array<std::int32_t, 4> around{};
    for (std::size_t index = 0; index < kSides.size(); ++index) {
        const std::int32_t nx = mark.x + registry::stepX(kSides[index]);
        const std::int32_t nz = mark.z + registry::stepZ(kSides[index]);
        Chunk* holder = (nx >> 4) == chunk.chunkX() && (nz >> 4) == chunk.chunkZ() ? &chunk : chunkAt(nx >> 4, nz >> 4);
        if (holder == nullptr) {
            // The neighbour is not here yet; its own install redoes this block from its side.
            return std::nullopt;
        }
        around[index] = holder->blockStateId(nx & 15, mark.y, nz & 15);
    }
    return around;
}

/// parity: `LevelChunk.postProcessGeneration` running `Block.updateFromNeighbourShapes` over what
/// the structure pieces marked, and the same pass `StructureTemplate.placeInWorld` ends with. Only
/// the connecting blocks and the stairs are redone here; a torch losing its support is a different
/// check and is not one of these.
void redoShapes(Chunk& chunk, const std::function<Chunk*(std::int32_t, std::int32_t)>& chunkAt) {
    static constexpr std::array<std::pair<registry::BlockFace, std::string_view>, 4> kSides{
        {{registry::BlockFace::North, "north"},
         {registry::BlockFace::South, "south"},
         {registry::BlockFace::West, "west"},
         {registry::BlockFace::East, "east"}}};
    for (const server::world::ChunkShapeUpdate& mark : chunk.shapeUpdates()) {
        const std::int32_t stateId = chunk.blockStateId(mark.x & 15, mark.y, mark.z & 15);
        const bool connects = registry::blockConnections::connects(stateId);
        if (!connects && !registry::blockConnections::isStairs(stateId)) {
            continue;
        }
        const std::optional<std::array<std::int32_t, 4>> around = horizontalNeighbours(chunk, mark, chunkAt);
        if (!around.has_value()) {
            continue;
        }
        std::map<std::string, std::string> wanted;
        if (connects) {
            for (std::size_t index = 0; index < kSides.size(); ++index) {
                wanted[std::string{kSides[index].second}] =
                    registry::blockConnections::connectsTo(stateId, (*around)[index], kSides[index].first) ? "true"
                                                                                                           : "false";
            }
        } else {
            const std::optional<std::string_view> shape =
                registry::blockConnections::stairShape(stateId, (*around)[0], (*around)[1], (*around)[2], (*around)[3]);
            if (!shape.has_value()) {
                continue;
            }
            wanted["shape"] = std::string{*shape};
        }
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
        if (block == nullptr) {
            continue;
        }
        const std::optional<std::int32_t> reshaped =
            registry::blockStateDefinitions::stateIdOf(*block, wanted, stateId);
        if (reshaped.has_value() && *reshaped != stateId) {
            chunk.setBlockStateId(mark.x & 15, mark.y, mark.z & 15, *reshaped);
        }
    }
}

} // namespace

Chunk& WorldState::installLocked(Chunk chunk) {
    const std::int32_t chunkX = chunk.chunkX();
    const std::int32_t chunkZ = chunk.chunkZ();
    const std::int64_t chunkKey = key(chunkX, chunkZ);
    if (Chunk* existing = chunkAtLocked(chunkKey); existing != nullptr) {
        return *existing;
    }

    Chunk& installed = *chunksByPos_.emplace(chunkKey, std::make_unique<Chunk>(std::move(chunk))).first->second;
    const PendingChunkDecorations::ChunkAt chunkAt = [this](std::int64_t otherKey) { return chunkAtLocked(otherKey); };
    pendingDecorations_.applyPendingGeneratedBlockWrites(installed,
                                                         PendingChunkDecorations::WritePhase::AfterDecoration);
    pendingDecorations_.applyPendingGeneratedBlockEntityWrites(installed);
    pendingDecorations_.applyPendingGeneratedBlockTicks(installed);
    const std::vector<server::world::ChunkGeneratedBlockWrite> spilledWrites = installed.takeGeneratedBlockWrites();
    pendingDecorations_.deliverGeneratedBlockWrites(spilledWrites, installed.decorationSequence(), chunkAt);
    pendingDecorations_.deliverGeneratedBlockEntityWrites(installed.takeGeneratedBlockEntityWrites(), chunkAt);
    const auto chunkAtPos = [this](std::int32_t otherX, std::int32_t otherZ) {
        return chunkAtLocked(key(otherX, otherZ));
    };
    redoShapes(installed, chunkAtPos);
    // Every sculk sensor the chunk brought with it, so a footstep next to one that nobody has ever
    // touched is still heard.
    sculkSensors_.rememberChunk(installed);
    // parity: the points of interest the generator's writes registered, which vanilla collects as
    // each block is placed. A write that landed in a neighbour already here is noted there too.
    poi_.chunkInstalled(installed);
    for (const server::world::ChunkGeneratedBlockWrite& write : spilledWrites) {
        if ((write.x >> 4) == chunkX && (write.z >> 4) == chunkZ) {
            continue;
        }
        if (const std::optional<std::int32_t> now = blockStateLocked(write.x, write.y, write.z); now.has_value()) {
            poi_.noteBlockState(poi::BlockPos{write.x, write.y, write.z}, *now);
        }
    }
    // The leaf distances settle once every write aimed at this chunk has landed, which is here and
    // not at the end of its own decoration: a neighbour's leaf arrives later and carries a stale one.
    const server::world::terrain::decoration::tree::leafDistances::BlockAt worldBlockAt =
        [this](std::int32_t x, std::int32_t y, std::int32_t z) -> std::int32_t {
        return blockStateLocked(x, y, z).value_or(0);
    };
    server::world::terrain::decoration::tree::leafDistances::settle(installed, worldBlockAt);
    for (const auto& [dx, dz] :
         std::array<std::pair<std::int32_t, std::int32_t>, 4>{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}}) {
        if (Chunk* neighbour = chunkAtPos(chunkX + dx, chunkZ + dz); neighbour != nullptr) {
            server::world::terrain::decoration::tree::leafDistances::settle(*neighbour, worldBlockAt);
        }
    }
    // parity: the same post-processing takes back a bush the finished light leaves too bright. The
    // features cannot know it while they run, because nothing has lit the chunk yet.
    const server::world::terrain::decoration::bushSurvival::NeighbourAt neighbourAt =
        [&chunkAtPos](std::int32_t otherX, std::int32_t otherZ) -> const server::world::Chunk* {
        return chunkAtPos(otherX, otherZ);
    };
    server::world::terrain::decoration::bushSurvival::settle(installed, neighbourAt, hasSkylight_);
    // parity: `LevelChunk.postProcessGeneration` ticks every fluid the aquifer marked, which is what
    // starts a lake running. Scheduling is this world's way of saying the same thing.
    for (const server::world::ChunkShapeUpdate& mark : installed.fluidUpdates()) {
        if (mark.y < installed.verticalBounds().minY() || mark.y > installed.verticalBounds().maxY()) {
            continue;
        }
        const std::int32_t stateId = installed.blockStateId(mark.x & 15, mark.y, mark.z & 15);
        if (const std::optional<FluidKind> kind = fluidFlow::kindOf(stateId); kind.has_value()) {
            installed.scheduleFluidTick(
                server::world::ChunkScheduledTick{.type = fluidId(*kind, isSourceFluid(stateId)),
                                                  .localX = mark.x & 15,
                                                  .y = mark.y,
                                                  .localZ = mark.z & 15,
                                                  .delay = 0});
        }
    }
    // A neighbour that was waiting on this chunk gets its own marked blocks redone now.
    for (const auto& [dx, dz] :
         std::array<std::pair<std::int32_t, std::int32_t>, 4>{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}}) {
        if (Chunk* neighbour = chunkAtPos(chunkX + dx, chunkZ + dz); neighbour != nullptr) {
            redoShapes(*neighbour, chunkAtPos);
        }
    }
    pendingDecorations_.deliverGeneratedBlockTicks(installed.takeGeneratedBlockTicks(), chunkAt);
    chunkVersion_.fetch_add(1);
    entityRegistry_.addPendingEntities(installed.takeSavedEntities());
    pendingDecorations_.addPendingGeneratedEntities(installed.takeGeneratedEntitiesMatching(
        [](const server::world::ChunkGeneratedEntity& entity) { return becomesLive(entity); }));

    // The neighbours were lit without this chunk, so their light let the sky in through where it
    // now stands. parity: vanilla relights a chunk's neighbourhood on load.
    for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
        for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
            if (offsetX == 0 && offsetZ == 0) {
                continue;
            }
            if (Chunk* neighbour = chunkAtLocked(key(chunkX + offsetX, chunkZ + offsetZ)); neighbour != nullptr) {
                neighbour->invalidateLight();
            }
        }
    }
    return installed;
}

Chunk& WorldState::install(Chunk chunk) {
    if (installOwner_ == nullptr) {
        mine("The set of chunks");
    }
    const WorldWriteLock lock{mutex_};
    return installLocked(std::move(chunk));
}

void WorldState::installLater(Chunk chunk, std::function<void()> installed) {
    sched::Owner* target = installOwner_ != nullptr ? installOwner_ : owner_;
    if (target == nullptr) {
        Chunk& placed = install(std::move(chunk));
        if (const std::function<void(Chunk&)> handler = chunkInstalledHandler()) {
            handler(placed);
        }
        if (installed) {
            installed();
        }
        return;
    }
    // Moved into the task rather than captured by reference: whoever asked for the chunk is not
    // waiting for it, and by the time the owner runs this their own copy is gone.
    target->submit([this, moved = std::make_shared<Chunk>(std::move(chunk)), installed = std::move(installed)]() {
        Chunk& placed = install(std::move(*moved));
        if (const std::function<void(Chunk&)> handler = chunkInstalledHandler()) {
            handler(placed);
        }
        if (installed) {
            installed();
        }
    });
}

std::optional<std::int32_t> WorldState::blockState(std::int32_t x, std::int32_t y, std::int32_t z) const {
    const WorldReadLock lock{mutex_};
    return blockStateLocked(x, y, z);
}

std::optional<std::int32_t> WorldState::blockStateLocked(std::int32_t x, std::int32_t y, std::int32_t z) const {
    const Chunk* chunk = chunkAtLocked(key(x >> 4, z >> 4));
    if (chunk == nullptr) {
        return std::nullopt;
    }
    // Above the sky or below the bedrock is not a block, which is what a mob that fell out of the
    // world asks about: the chunk refuses the question rather than answering it.
    if (y < chunk->verticalBounds().minY() || y > chunk->verticalBounds().maxY()) {
        return std::nullopt;
    }
    return chunk->blockStateId(x & 15, y, z & 15);
}

entity::entityCollision::BlockAt WorldState::blockAccessor() const {
    return [this](std::int32_t x, std::int32_t y, std::int32_t z) { return blockState(x, y, z); };
}

entity::entityCollision::BlockAt WorldState::lockedBlockAccessor() const {
    return [this](std::int32_t x, std::int32_t y, std::int32_t z) { return blockStateLocked(x, y, z); };
}

bool WorldState::setBlockState(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                               SideEffects sideEffects) {
    mine("A block");
    const WorldWriteLock lock{mutex_};
    Chunk* chunk = chunkAtLocked(key(x >> 4, z >> 4));
    if (chunk == nullptr) {
        return false;
    }
    // parity: `Level.setBlock` refuses a position outside build height rather than throwing, which
    // is what a plugin or a piston reaching past the sky asks for. The frozen Kotlin let the chunk
    // throw instead, and the throw came out on the tick thread.
    if (y < chunk->verticalBounds().minY() || y > chunk->verticalBounds().maxY()) {
        return false;
    }
    // parity: `BlockEntity.preRemoveSideEffects`, which every container runs when its block goes.
    // Queued rather than spawned because this call already holds the world's lock, and done before
    // the write, which is what takes the block entity away. Skipped when the flags carry 256.
    //
    // Cheap tests first: this is the hottest write in the server and usually the state did not move.
    const std::int32_t was = chunk->blockStateId(x & 15, y, z & 15);
    const auto blockChanges = [](std::int32_t before, std::int32_t after) {
        const wire::common::Identifier* from = registry::blockStateDefinitions::blockOf(before);
        const wire::common::Identifier* to = registry::blockStateDefinitions::blockOf(after);
        // parity: `shouldChangedStateKeepBlockEntity`: a copper chest that weathers, waxes or is
        // scraped keeps its block entity, so nothing falls out of it either.
        return from != nullptr && to != nullptr && *from != *to &&
               !registry::weathering::keepsBlockEntity(from->path(), to->path());
    };
    if (sideEffects == SideEffects::kRun && was != stateId && chunk->blockEntityCount() > 0 &&
        blockChanges(was, stateId)) {
        const server::world::ChunkBlockEntity* held = chunk->blockEntityAt(x & 15, y, z & 15);
        // parity: `ShulkerBoxBlockEntity.preRemoveSideEffects` is empty on purpose, because what the
        // box held goes with the box. It is the one container in the game that keeps its contents.
        const bool keepsContents = held != nullptr && shulkers::isShulkerBox(held->type().path());
        const std::optional<std::int32_t> size =
            held == nullptr || keepsContents ? std::nullopt : containers::sizeOf(held->type().path());
        if (size.has_value()) {
            Spilled falling{.x = x, .y = y, .z = z, .stacks = {}, .recipesUsed = {}};
            if (furnaces::cookerFor(held->type().path()).has_value()) {
                falling.recipesUsed = furnaces::recipesUsedIn(held->data());
            }
            blockInventory::Slots slots = blockInventory::readFor(held->type().path(), held->data(), *size);
            if (const std::string_view table = held->data().getString("LootTable"); !table.empty()) {
                falling.lootTable = std::string{table};
                falling.lootSeed = held->data().getLong("LootTableSeed");
                falling.lootSlots = std::move(slots);
            } else {
                for (const std::optional<server::world::loot::ItemStack>& slot : slots) {
                    if (slot.has_value() && slot->count > 0) {
                        falling.stacks.push_back(*slot);
                    }
                }
            }
            if (!falling.stacks.empty() || !falling.recipesUsed.empty() || falling.lootTable.has_value()) {
                spilled_.push_back(std::move(falling));
            }
        }
    }
    const bool changed = chunk->setBlockStateId(x & 15, y, z & 15, stateId);
    if (changed) {
        poi_.noteBlockChange(poi::BlockPos{x, y, z}, was, stateId);
        // A skylight opening or a torch reaches across chunk borders. Cached whole-chunk light is
        // cheaper than incremental light here, but every cached neighbour that could have read the
        // old block must be discarded together.
        for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
            for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
                if (Chunk* neighbour = chunkAtLocked(key((x >> 4) + offsetX, (z >> 4) + offsetZ));
                    neighbour != nullptr) {
                    neighbour->invalidateLight();
                }
            }
        }
    }
    sculkSensors_.remember(x, y, z, stateId);
    return true;
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
        // Nothing to advance, and most chunks have nothing: the same walk without this ran the
        // whole loaded world through two empty vectors twenty times a second.
        if (chunk->scheduledTickCount() == 0) {
            continue;
        }
        // parity: a chunk that is loaded and not ticking is frozen: `ChunkMap` takes its
        // `LevelTicks` container away, so its turns neither run nor count down. Ours used to walk
        // every loaded chunk, and a clock three chunks from spawn kept going with nobody connected
        // where 26.2 stops it (`spawnsigue`).
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
    // parity: `ScheduledTick.INTRA_TICK_DRAIN_ORDER`, priority first and then the order they were
    // booked in. Without it the order was whichever chunk the map walked first, so two components
    // due on the same turn ran in an order that had nothing to do with the game.
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
        // The same two gates the passes use: nothing booked here, or a chunk that is frozen because
        // nobody is ticking it, and its turns neither run nor count down.
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
    return itemPhysics_.spawnItem(std::move(dropped));
}

std::vector<DroppedItem> WorldState::items() const { return itemPhysics_.items(); }

std::vector<DroppedItem> WorldState::nearbyItems(double x, double y, double z, double radius) const {
    return itemPhysics_.nearbyItems(x, y, z, radius);
}

bool WorldState::shiftItem(std::int32_t entityId, double dx, double dy, double dz) {
    return itemPhysics_.shiftItem(entityId, dx, dy, dz);
}

bool WorldState::hurtItem(const std::int32_t entityId, const std::int32_t amount) {
    return itemPhysics_.hurtItem(entityId, amount);
}

std::optional<DroppedItem> WorldState::removeItem(std::int32_t entityId) {
    mine("A dropped item");
    return itemPhysics_.removeItem(entityId);
}

std::vector<std::int32_t> WorldState::tickItems(const sched::Region* region) { return itemPhysics_.tickItems(region); }

std::vector<entity::ServerEntity*> WorldState::entities() const { return entityRegistry_.entities(); }

bool WorldState::hasNaturalSpawnEntityObstruction(const entity::Aabb& spawnBox, const entity::Aabb& entityBox) const {
    return entityRegistry_.hasNaturalSpawnObstruction(spawnBox, entityBox);
}

std::vector<entity::ServerEntity*> WorldState::entitiesIn(std::int32_t chunkX, std::int32_t chunkZ) const {
    return entityRegistry_.entitiesIn(chunkX, chunkZ);
}

std::vector<std::pair<std::int32_t, std::int32_t>> WorldState::chunksWithEntities() const {
    std::vector<std::pair<std::int32_t, std::int32_t>> chunks = entityRegistry_.chunksWithEntities();
    // An item on the floor is an entity too. Leaving it out meant a chunk whose only change was a
    // stack rolling across it was never written down, and what came back was where it used to be.
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

std::vector<wire::nbt::NbtCompound> WorldState::takePendingEntities() { return entityRegistry_.takePendingEntities(); }

std::vector<server::world::ChunkGeneratedEntity> WorldState::takePendingGeneratedEntities() {
    return pendingDecorations_.takePendingGeneratedEntities();
}

entity::ServerEntity* WorldState::entity(std::int32_t entityId) const { return entityRegistry_.entity(entityId); }

entity::ServerEntity& WorldState::spawnEntity(std::unique_ptr<entity::ServerEntity> entity) {
    mine("An entity");
    return entityRegistry_.spawnEntity(std::move(entity));
}

std::vector<std::int32_t> WorldState::collectRemovedEntities() { return entityRegistry_.collectRemovedEntities(); }

void WorldState::primeTnt(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view event) {
    primedTntQueue_.primeTnt(x, y, z, event);
}

std::vector<PrimedTntQueue::Primed> WorldState::takePrimedTnt() { return primedTntQueue_.takePrimedTnt(); }

std::vector<WorldState::Spilled> WorldState::takeSpilledContents() {
    const WorldWriteLock lock{mutex_};
    return std::exchange(spilled_, {});
}

} // namespace basalt::server::runtime::world
