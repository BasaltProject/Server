#include "WorldStateShared.hpp"

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

namespace {

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

/// parity: `LevelChunk.postProcessGeneration` running `Block.updateFromNeighbourShapes` over what.
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
    chunk.share();

    Chunk& installed = *chunksByPos_.emplace(chunkKey, std::make_unique<Chunk>(std::move(chunk))).first->second;
    blockVersion_.fetch_add(1, std::memory_order_release);
    const PendingChunkDecorations::ChunkAt chunkAt = [this](std::int64_t otherKey) { return chunkAtLocked(otherKey); };
    const std::vector<server::world::ChunkGeneratedBlockWrite> spilledWrites = installed.takeGeneratedBlockWrites();
    PendingChunkDecorations::deliverGeneratedBlockWrites(spilledWrites, chunkAt);
    PendingChunkDecorations::deliverGeneratedBlockEntityWrites(installed.takeGeneratedBlockEntityWrites(), chunkAt);
    const auto chunkAtPos = [this](std::int32_t otherX, std::int32_t otherZ) {
        return chunkAtLocked(key(otherX, otherZ));
    };
    redoShapes(installed, chunkAtPos);
    sculkSensors_.rememberChunk(installed);
    // parity: the points of interest the generator's writes registered, which vanilla collects as.
    poi_.chunkInstalled(installed);
    for (const server::world::ChunkGeneratedBlockWrite& write : spilledWrites) {
        if ((write.x >> 4) == chunkX && (write.z >> 4) == chunkZ) {
            continue;
        }
        if (const std::optional<std::int32_t> now = blockStateLocked(write.x, write.y, write.z); now.has_value()) {
            poi_.noteBlockState(poi::BlockPos{write.x, write.y, write.z}, *now);
        }
    }
    // parity: a debug world places its grid without updates, so nothing is settled after it.
    if (settlesGeneration_) {
        // Most reads fall in the one chunk the last one did, so that chunk is kept rather than found again.
        const server::world::terrain::decoration::tree::leafDistances::BlockAt worldBlockAt =
            [this, cachedKey = std::int64_t{0}, cached = static_cast<Chunk*>(nullptr),
             cachedValid = false](std::int32_t x, std::int32_t y, std::int32_t z) mutable -> std::int32_t {
            const std::int64_t wanted = key(x >> 4, z >> 4);
            if (!cachedValid || cachedKey != wanted) {
                cached = chunkAtLocked(wanted);
                cachedKey = wanted;
                cachedValid = true;
            }
            if (cached == nullptr || y < cached->verticalBounds().minY() || y > cached->verticalBounds().maxY()) {
                return 0;
            }
            return cached->blockStateId(x & 15, y, z & 15);
        };
        const bool touchesNeighbours =
            server::world::terrain::decoration::tree::leafDistances::mayChangeNeighbours(installed);
        if (!installed.loadedComplete()) {
            server::world::terrain::decoration::tree::leafDistances::settle(installed, worldBlockAt);
        }
        // All eight: a diagonal neighbour's leaves are within six blocks of this chunk's logs too.
        for (std::int32_t dx = -1; dx <= 1 && touchesNeighbours; ++dx) {
            for (std::int32_t dz = -1; dz <= 1; ++dz) {
                if (Chunk* neighbour = chunkAtPos(chunkX + dx, chunkZ + dz);
                    neighbour != nullptr && (dx != 0 || dz != 0) && !neighbour->loadedComplete()) {
                    server::world::terrain::decoration::tree::leafDistances::settle(*neighbour, worldBlockAt);
                }
            }
        }
        // parity: the same post-processing takes back a bush the finished light leaves too bright. The.
        const server::world::terrain::decoration::bushSurvival::NeighbourAt neighbourAt =
            [&chunkAtPos](std::int32_t otherX, std::int32_t otherZ) -> const server::world::Chunk* {
            return chunkAtPos(otherX, otherZ);
        };
        if (!installed.loadedComplete()) {
            server::world::terrain::decoration::bushSurvival::settle(installed, neighbourAt, hasSkylight_);
        }
    }
    // parity: `LevelChunk.postProcessGeneration` ticks every fluid the aquifer marked.
    for (const std::span<const server::world::ChunkShapeUpdate> marks :
         {installed.fluidUpdates(), installed.shapeUpdates()}) {
        for (const server::world::ChunkShapeUpdate& mark : marks) {
            if (mark.y < installed.verticalBounds().minY() || mark.y > installed.verticalBounds().maxY() ||
                (mark.x >> 4) != chunkX || (mark.z >> 4) != chunkZ) {
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
            } else if (fallingBlocks::falls(stateId)) {
                // parity: `FallingBlock.updateShape`, booked whether or not there is room below.
                if (const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
                    block != nullptr) {
                    installed.scheduleBlockTick(
                        server::world::ChunkScheduledTick{.type = *block,
                                                          .localX = mark.x & 15,
                                                          .y = mark.y,
                                                          .localZ = mark.z & 15,
                                                          .delay = fallingBlocks::kFallDelayTicks});
                }
            }
        }
    }
    // A neighbour that was waiting on this chunk gets its own marked blocks redone now.
    for (const auto& [dx, dz] :
         std::array<std::pair<std::int32_t, std::int32_t>, 4>{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}}) {
        if (Chunk* neighbour = chunkAtPos(chunkX + dx, chunkZ + dz); neighbour != nullptr) {
            redoShapes(*neighbour, chunkAtPos);
        }
    }
    PendingChunkDecorations::deliverGeneratedBlockTicks(installed.takeGeneratedBlockTicks(), chunkAt);
    chunkVersion_.fetch_add(1);
    entityRegistry_.addPendingEntities(installed.takeSavedEntities());
    pendingDecorations_.addPendingGeneratedEntities(installed.takeGeneratedEntitiesMatching(
        [](const server::world::ChunkGeneratedEntity& entity) { return becomesLive(entity); }));

    if (onNeighbourhoodComplete_) {
        for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
            for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
                const std::int32_t candidateX = chunkX + offsetX;
                const std::int32_t candidateZ = chunkZ + offsetZ;
                bool complete = chunkAtLocked(key(candidateX, candidateZ)) != nullptr;
                for (std::int32_t aroundZ = -1; aroundZ <= 1 && complete; ++aroundZ) {
                    for (std::int32_t aroundX = -1; aroundX <= 1 && complete; ++aroundX) {
                        complete = chunkAtLocked(key(candidateX + aroundX, candidateZ + aroundZ)) != nullptr;
                    }
                }
                if (complete) {
                    onNeighbourhoodComplete_(candidateX, candidateZ, false);
                }
            }
        }
    }

    // now stands. parity: vanilla relights a chunk's neighbourhood on load.
    for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
        for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
            if (offsetX == 0 && offsetZ == 0) {
                continue;
            }
            if (Chunk* neighbour = chunkAtLocked(key(chunkX + offsetX, chunkZ + offsetZ)); neighbour != nullptr) {
                neighbour->markLightStale();
                if (onNeighbourhoodComplete_) {
                    onNeighbourhoodComplete_(chunkX + offsetX, chunkZ + offsetZ, true);
                }
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
    {
        const std::lock_guard<std::mutex> lock{installMutex_};
        pendingInstalls_.push_back(PendingInstall{std::make_shared<Chunk>(std::move(chunk)), std::move(installed)});
        if (installPassQueued_) {
            return;
        }
        installPassQueued_ = true;
    }
    target->submit([this, target] { installPass(target); });
}

void WorldState::installPass(sched::Owner* target) {
    // Settling leaves costs milliseconds a chunk: taking every waiting chunk in one tick starves the owner's queue.
    constexpr std::chrono::milliseconds kBudget{8};
    const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    while (true) {
        PendingInstall next;
        {
            const std::lock_guard<std::mutex> lock{installMutex_};
            if (pendingInstalls_.empty()) {
                installPassQueued_ = false;
                return;
            }
            next = std::move(pendingInstalls_.front());
            pendingInstalls_.pop_front();
        }
        try {
            Chunk& placed = install(std::move(*next.chunk));
            if (const std::function<void(Chunk&)> handler = chunkInstalledHandler()) {
                handler(placed);
            }
            if (next.installed) {
                next.installed();
            }
        } catch (...) {
            // The ones still waiting must not be stranded behind a chunk that failed.
            target->submit([this, target] { installPass(target); });
            throw;
        }
        if (std::chrono::steady_clock::now() - started >= kBudget) {
            break;
        }
    }
    target->submit([this, target] { installPass(target); });
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
                               SideEffects sideEffects, PoiTracking poiTracking) {
    mine("A block");
    const WorldWriteLock lock{mutex_};
    blockVersion_.fetch_add(1, std::memory_order_release);
    Chunk* chunk = chunkAtLocked(key(x >> 4, z >> 4));
    if (chunk == nullptr) {
        return false;
    }
    // parity: `Level.setBlock` refuses a position outside build height rather than throwing.
    if (y < chunk->verticalBounds().minY() || y > chunk->verticalBounds().maxY()) {
        return false;
    }
    // parity: `BlockEntity.preRemoveSideEffects`.
    const std::int32_t was = chunk->blockStateId(x & 15, y, z & 15);
    const auto blockChanges = [](std::int32_t before, std::int32_t after) {
        const wire::common::Identifier* from = registry::blockStateDefinitions::blockOf(before);
        const wire::common::Identifier* to = registry::blockStateDefinitions::blockOf(after);
        // parity: `shouldChangedStateKeepBlockEntity`: a copper chest that weathers, waxes or is.
        return from != nullptr && to != nullptr && *from != *to &&
               !registry::weathering::keepsBlockEntity(from->path(), to->path());
    };
    if (sideEffects == SideEffects::kRun && was != stateId && chunk->blockEntityCount() > 0 &&
        blockChanges(was, stateId)) {
        const server::world::ChunkBlockEntity* held = chunk->blockEntityAt(x & 15, y, z & 15);
        // parity: `ShulkerBoxBlockEntity.preRemoveSideEffects` is empty on purpose.
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
        // parity: `LevelChunk.setBlockState` gives a state that owns a block entity one whenever.
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
        if (block != nullptr && block->path() != "moving_piston" &&
            registry::blockEntityTypes::ofBlock(*block) != nullptr &&
            chunk->blockEntityAt(x & 15, y, z & 15) == nullptr) {
            chunk->setBlockEntity(
                server::world::ChunkBlockEntity{x & 15, y, z & 15, *block, blockEntityDefaults::of(*block, stateId)});
        }
        if (poiTracking == PoiTracking::kTrack) {
            poi_.noteBlockChange(poi::BlockPos{x, y, z}, was, stateId);
        }
        const std::span<const std::int32_t> before = registry::blockShapes::collisionBoxes(was);
        const std::span<const std::int32_t> after = registry::blockShapes::collisionBoxes(stateId);
        const auto moving = [](std::int32_t state) {
            const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(state);
            return block != nullptr && block->path() == "moving_piston";
        };
        if (!std::ranges::equal(before, after) || moving(was) || moving(stateId)) {
            shapeChanges_[shapeChangeCount_ % kShapeChangeMemory] = {x, y, z};
            ++shapeChangeCount_;
        }
        if (server::world::changesLight(was, stateId)) {
            for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
                for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
                    if (Chunk* neighbour = chunkAtLocked(key((x >> 4) + offsetX, (z >> 4) + offsetZ));
                        neighbour != nullptr) {
                        neighbour->markLightStale();
                        if (onNeighbourhoodComplete_) {
                            onNeighbourhoodComplete_(neighbour->chunkX(), neighbour->chunkZ(), true);
                        }
                    }
                }
            }
        }
    }
    sculkSensors_.remember(x, y, z, stateId);
    return true;
}

bool WorldState::copySection(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ,
                             std::span<std::int32_t> out) const {
    const WorldReadLock lock{mutex_};
    const Chunk* chunk = chunkAtLocked(key(chunkX, chunkZ));
    if (chunk == nullptr || out.size() < server::world::ChunkSection::kBlocks) {
        return false;
    }
    const std::int32_t index = sectionY - chunk->verticalBounds().minSection();
    if (index < 0 || static_cast<std::size_t>(index) >= chunk->sectionCount()) {
        return false;
    }
    chunk->section(static_cast<std::size_t>(index)).copyBlockStateIdsInto(out);
    return true;
}

bool WorldState::copySections(std::int32_t chunkX, std::int32_t chunkZ, std::span<std::int32_t> out) const {
    constexpr std::size_t kBlocks = server::world::ChunkSection::kBlocks;
    const WorldReadLock lock{mutex_};
    const Chunk* chunk = chunkAtLocked(key(chunkX, chunkZ));
    if (chunk == nullptr || out.size() < chunk->sectionCount() * kBlocks) {
        return false;
    }
    for (std::size_t index = 0; index < chunk->sectionCount(); ++index) {
        chunk->section(index).copyBlockStateIdsInto(out.subspan(index * kBlocks, kBlocks));
    }
    return true;
}

std::int32_t WorldState::replaceSection(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ,
                                        std::span<const std::int32_t> next, std::span<std::int32_t> previous,
                                        std::vector<std::uint16_t>* changedOffsets) {
    mine("A section");
    constexpr std::size_t kBlocks = server::world::ChunkSection::kBlocks;
    const WorldWriteLock lock{mutex_};
    blockVersion_.fetch_add(1, std::memory_order_release);
    Chunk* chunk = chunkAtLocked(key(chunkX, chunkZ));
    if (chunk == nullptr || next.size() < kBlocks) {
        return -1;
    }
    const std::int32_t index = sectionY - chunk->verticalBounds().minSection();
    if (index < 0 || static_cast<std::size_t>(index) >= chunk->sectionCount()) {
        return -1;
    }
    std::vector<std::int32_t> before(kBlocks);
    chunk->section(static_cast<std::size_t>(index)).copyBlockStateIdsInto(before);
    if (previous.size() >= kBlocks) {
        std::ranges::copy(before, previous.begin());
    }
    std::vector<std::int32_t> merged(kBlocks);
    std::vector<std::uint16_t> changed;
    for (std::size_t at = 0; at < kBlocks; ++at) {
        merged[at] = next[at] < 0 ? before[at] : next[at];
        if (merged[at] != before[at]) {
            changed.push_back(static_cast<std::uint16_t>(at));
        }
    }
    if (changed.empty()) {
        return 0;
    }
    if (changedOffsets != nullptr) {
        *changedOffsets = changed;
    }
    chunk->replaceSectionStates(static_cast<std::size_t>(index), merged);
    const std::int32_t baseX = chunkX << 4;
    const std::int32_t baseY = sectionY << 4;
    const std::int32_t baseZ = chunkZ << 4;
    bool lightChanged = false;
    std::size_t shapes = 0;
    const auto moving = [](std::int32_t state) {
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(state);
        return block != nullptr && block->path() == "moving_piston";
    };
    for (const std::uint16_t at : changed) {
        const std::int32_t was = before[at];
        const std::int32_t stateId = merged[at];
        const std::int32_t x = baseX + (at & 15);
        const std::int32_t y = baseY + (at >> 8);
        const std::int32_t z = baseZ + ((at >> 4) & 15);
        // parity: `LevelChunk.setBlockState` gives a state that owns a block entity one whenever.
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
        if (block != nullptr && block->path() != "moving_piston" &&
            registry::blockEntityTypes::ofBlock(*block) != nullptr &&
            chunk->blockEntityAt(x & 15, y, z & 15) == nullptr) {
            chunk->setBlockEntity(
                server::world::ChunkBlockEntity{x & 15, y, z & 15, *block, blockEntityDefaults::of(*block, stateId)});
        }
        poi_.noteBlockChange(poi::BlockPos{x, y, z}, was, stateId);
        if (!std::ranges::equal(registry::blockShapes::collisionBoxes(was),
                                registry::blockShapes::collisionBoxes(stateId)) ||
            moving(was) || moving(stateId)) {
            ++shapes;
        }
        lightChanged = lightChanged || server::world::changesLight(was, stateId);
        if (block != nullptr && (block->path() == "sculk_sensor" || block->path() == "calibrated_sculk_sensor")) {
            sculkSensors_.remember(x, y, z, stateId);
        } else if (registry::blockStateDefinitions::blockOf(was) != nullptr) {
            sculkSensors_.remember(x, y, z, stateId);
        }
    }
    if (shapes > 0) {
        // The last kShapeChangeMemory entries are kept exactly; the count moves by all so readers see an overflow.
        std::size_t skipped = 0;
        std::size_t kept = 0;
        for (std::size_t i = changed.size(); i-- > 0 && kept < kShapeChangeMemory;) {
            const std::uint16_t at = changed[i];
            shapeChanges_[(shapeChangeCount_ + kept) % kShapeChangeMemory] = {baseX + (at & 15), baseY + (at >> 8),
                                                                              baseZ + ((at >> 4) & 15)};
            ++kept;
        }
        static_cast<void>(skipped);
        shapeChangeCount_ += std::max(kept, shapes);
    }
    if (lightChanged) {
        for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
            for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
                if (Chunk* neighbour = chunkAtLocked(key(chunkX + offsetX, chunkZ + offsetZ)); neighbour != nullptr) {
                    neighbour->markLightStale();
                    if (onNeighbourhoodComplete_) {
                        onNeighbourhoodComplete_(neighbour->chunkX(), neighbour->chunkZ(), true);
                    }
                }
            }
        }
    }
    return static_cast<std::int32_t>(changed.size());
}

} // namespace basalt::server::runtime::world
