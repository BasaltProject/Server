#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace basalt::server::world {
namespace {

namespace registry = basalt::server::registry;

[[nodiscard]] bool matchesUncached(HeightmapType type, std::int32_t stateId) {
    switch (type) {
    case HeightmapType::WorldSurface:
        return !registry::blocks::isAir(stateId);
    case HeightmapType::OceanFloor:
        return registry::blockShapes::blocksMotion(stateId);
    case HeightmapType::MotionBlocking:
        return registry::blockShapes::blocksMotion(stateId) || registry::blockFluids::hasFluid(stateId);
    default:
        // The leaves question comes from the block family table rather than a range of state ids:
        // checked against the arithmetic it replaces, the two agree on all 32,366 states.
        return !registry::blockConnections::isLeaves(stateId) &&
               (registry::blockShapes::blocksMotion(stateId) || registry::blockFluids::hasFluid(stateId));
    }
}

/// One answer per state id per heightmap, built once.
struct HeightmapTables {
    std::int32_t stateCount = 0;
    std::array<std::vector<bool>, kHeightmapCount> matches;
};

[[nodiscard]] const HeightmapTables& tables() {
    static const HeightmapTables built = [] {
        HeightmapTables tables;
        tables.stateCount = registry::blocks::stateCount();
        for (std::size_t type = 0; type < kHeightmapCount; ++type) {
            tables.matches[type].resize(static_cast<std::size_t>(tables.stateCount));
            for (std::int32_t stateId = 0; stateId < tables.stateCount; ++stateId) {
                tables.matches[type][static_cast<std::size_t>(stateId)] =
                    matchesUncached(static_cast<HeightmapType>(type), stateId);
            }
        }
        return tables;
    }();
    return built;
}

/// parity: `LevelChunk.setBlockState`, which drops the block entity only when the *block* changes.
///
/// A furnace lighting up, a campfire being put out or a chest taking on water keeps its own: they
/// are the same block in another state, and what is inside them does not fall out.
[[nodiscard]] bool changesBlock(std::int32_t oldStateId, std::int32_t newStateId) {
    if (oldStateId == newStateId) {
        return false;
    }
    const wire::common::Identifier* before = registry::blockStateDefinitions::blockOf(oldStateId);
    const wire::common::Identifier* after = registry::blockStateDefinitions::blockOf(newStateId);
    if (before == nullptr || after == nullptr) {
        return before != after;
    }
    return *before != *after;
}

} // namespace

bool heightmapMatches(HeightmapType type, std::int32_t stateId) {
    const HeightmapTables& built = tables();
    // A state id past what the tables were sized with falls back rather than reading off the end.
    if (stateId < 0 || stateId >= built.stateCount) {
        return matchesUncached(type, stateId);
    }
    return built.matches[static_cast<std::size_t>(type)][static_cast<std::size_t>(stateId)];
}

Chunk::Chunk(std::int32_t chunkX, std::int32_t chunkZ, ChunkHeight bounds)
    : chunkX_(chunkX), chunkZ_(chunkZ), bounds_(bounds), sections_(static_cast<std::size_t>(bounds.sectionCount())) {
    if (!bounds.isValid()) {
        throw std::invalid_argument{"chunk bounds are not ones a chunk can be stored in"};
    }
    for (std::vector<std::int32_t>& map : heights_) {
        map.assign(16 * 16, bounds.minY());
    }
}

std::size_t Chunk::sectionIndexFor(std::int32_t y) const {
    if (y < bounds_.minY() || y > bounds_.maxY()) {
        throw std::out_of_range{"y out of world bounds"};
    }
    return static_cast<std::size_t>((y >> 4) - bounds_.minSection());
}

std::int32_t Chunk::blockStateId(std::int32_t x, std::int32_t y, std::int32_t z) const {
    return sections_[sectionIndexFor(y)].blockStateId(x, y & 15, z);
}

bool Chunk::setBlockStateId(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    ChunkSection& section = sections_[sectionIndexFor(y)];
    if (!blockWriteObserver_) {
        if (!blockEntities_.empty() && changesBlock(section.blockStateId(x, y & 15, z), stateId)) {
            removeBlockEntityAt(x, y, z);
        }
        if (!section.setBlockStateId(x, y & 15, z, stateId)) {
            return false;
        }
        dirty_ = true;
        updateHeights(x, y, z, stateId);
        invalidateLight();
        return true;
    }

    // An observed write reports the attempt whether or not it changed anything, which is what a
    // feature that records what it placed needs to see.
    const std::int32_t oldStateId = section.blockStateId(x, y & 15, z);
    section.setBlockStateId(x, y & 15, z, stateId);
    const bool changed = oldStateId != stateId;
    if (changed) {
        dirty_ = true;
        updateHeights(x, y, z, stateId);
        if (changesBlock(oldStateId, stateId)) {
            removeBlockEntityAt(x, y, z);
        }
    }
    blockWriteObserver_(x, y, z, oldStateId, stateId);
    if (changed) {
        invalidateLight();
    }
    return changed;
}

void Chunk::removeBlockEntityAt(std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::int32_t removedKey = blockEntityKey(x, y, z);
    if (blockEntities_.erase(removedKey) != 0) {
        std::erase(blockEntityOrder_, removedKey);
    }
}

const Chunk::PackedLight& Chunk::skyLight(const std::function<PackedLight()>& compute) {
    if (!skyLight_.has_value()) {
        skyLight_ = compute();
    }
    return *skyLight_;
}

const Chunk::PackedLight& Chunk::blockLight(const std::function<PackedLight()>& compute) {
    if (!blockLight_.has_value()) {
        blockLight_ = compute();
    }
    return *blockLight_;
}

const Chunk::PackedLight* Chunk::cachedSkyLight() const { return skyLight_.has_value() ? &*skyLight_ : nullptr; }

const Chunk::PackedLight* Chunk::cachedBlockLight() const { return blockLight_.has_value() ? &*blockLight_ : nullptr; }

void Chunk::invalidateLight() {
    skyLight_.reset();
    blockLight_.reset();
}

void Chunk::increaseInhabitedTime(std::int64_t delta) {
    if (delta < 0) {
        throw std::invalid_argument{"inhabited time cannot move backwards"};
    }
    if (delta == 0) {
        return;
    }
    inhabitedTime_ += delta;
    dirty_ = true;
}

void Chunk::restoreInhabitedTime(std::int64_t value) { inhabitedTime_ = std::max<std::int64_t>(value, 0); }

void Chunk::setBiomeId(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ, std::int32_t biomeId) {
    if (quartY < 0 || quartY >= bounds_.height() / 4) {
        throw std::out_of_range{"quartY out of world bounds"};
    }
    sections_[static_cast<std::size_t>(quartY >> 2)].setBiomeId(quartX, quartY & 3, quartZ, biomeId);
    dirty_ = true;
}

std::int32_t Chunk::biomeId(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) const {
    if (quartY < 0 || quartY >= bounds_.height() / 4) {
        throw std::out_of_range{"quartY out of world bounds"};
    }
    return sections_[static_cast<std::size_t>(quartY >> 2)].biomeId(quartX, quartY & 3, quartZ);
}

std::vector<std::int32_t> Chunk::heightmap(HeightmapType type) const {
    if (!heightsValid_) {
        rebuildHeights();
    }
    return heights_[static_cast<std::size_t>(type)];
}

std::int32_t Chunk::heightAt(std::int32_t x, std::int32_t z, HeightmapType type) const {
    if (!heightsValid_) {
        rebuildHeights();
    }
    return heights_[static_cast<std::size_t>(type)][static_cast<std::size_t>(x + z * 16)];
}

void Chunk::freezeWorldgenHeights() {
    if (!heightsValid_) {
        rebuildHeights();
    }
    worldgenHeights_ = heights_;
}

std::int32_t Chunk::worldgenHeightAt(std::int32_t x, std::int32_t z, HeightmapType type) const {
    if (!worldgenHeights_.has_value()) {
        return heightAt(x, z, type);
    }
    return (*worldgenHeights_)[static_cast<std::size_t>(type)][static_cast<std::size_t>(x + z * 16)];
}

void Chunk::invalidateHeightmaps() {
    heightsValid_ = false;
    invalidateLight();
}

void Chunk::setBlockEntity(ChunkBlockEntity blockEntity) {
    if (blockEntity.y() < bounds_.minY() || blockEntity.y() > bounds_.maxY()) {
        throw std::out_of_range{"block entity outside chunk bounds"};
    }
    const std::int32_t key = blockEntityKey(blockEntity.localX(), blockEntity.y(), blockEntity.localZ());
    if (blockEntities_.insert_or_assign(key, std::move(blockEntity)).second) {
        blockEntityOrder_.push_back(key);
    }
    dirty_ = true;
}

const ChunkBlockEntity* Chunk::blockEntityAt(std::int32_t localX, std::int32_t y, std::int32_t localZ) const {
    const auto found = blockEntities_.find(blockEntityKey(localX, y, localZ));
    return found == blockEntities_.end() ? nullptr : &found->second;
}

ChunkBlockEntity* Chunk::blockEntityAt(std::int32_t localX, std::int32_t y, std::int32_t localZ) {
    const auto found = blockEntities_.find(blockEntityKey(localX, y, localZ));
    return found == blockEntities_.end() ? nullptr : &found->second;
}

std::vector<ChunkBlockEntity> Chunk::blockEntities() const {
    std::vector<ChunkBlockEntity> all;
    all.reserve(blockEntities_.size());
    for (const std::int32_t key : blockEntityOrder_) {
        all.push_back(blockEntities_.at(key));
    }
    return all;
}

void Chunk::scheduleBlockTick(const ChunkScheduledTick& tick) { scheduleTick(blockTicks_, tick); }

void Chunk::scheduleFluidTick(const ChunkScheduledTick& tick) { scheduleTick(fluidTicks_, tick); }

void Chunk::scheduleTick(std::vector<ChunkScheduledTick>& ticks, const ChunkScheduledTick& tick) {
    if (tick.y < bounds_.minY() || tick.y > bounds_.maxY()) {
        throw std::out_of_range{"scheduled tick outside chunk bounds"};
    }
    const bool duplicate = std::ranges::any_of(ticks, [&tick](const ChunkScheduledTick& one) {
        return one.type == tick.type && one.localX == tick.localX && one.y == tick.y && one.localZ == tick.localZ;
    });
    if (duplicate) {
        return;
    }
    ticks.push_back(tick);
    dirty_ = true;
}

void Chunk::advanceBlockTicks(std::vector<ChunkScheduledTick>& due) { advance(blockTicks_, due); }

void Chunk::advanceFluidTicks(std::vector<ChunkScheduledTick>& due) { advance(fluidTicks_, due); }

void Chunk::advance(std::vector<ChunkScheduledTick>& pending, std::vector<ChunkScheduledTick>& due) {
    if (pending.empty()) {
        return;
    }
    std::size_t index = 0;
    while (index < pending.size()) {
        if (pending[index].delay <= 1) {
            due.push_back(pending[index]);
            pending.erase(pending.begin() + static_cast<std::ptrdiff_t>(index));
        } else {
            pending[index].delay--;
            index++;
        }
    }
}

void Chunk::setSavedEntities(std::vector<wire::nbt::NbtCompound> entities) {
    if (savedEntities_.empty() && entities.empty()) {
        return;
    }
    savedEntities_ = std::move(entities);
    dirty_ = true;
}

std::vector<wire::nbt::NbtCompound> Chunk::takeSavedEntities() { return std::exchange(savedEntities_, {}); }

void Chunk::addGeneratedEntity(ChunkGeneratedEntity entity) {
    generatedEntities_.push_back(std::move(entity));
    dirty_ = true;
}

std::vector<ChunkGeneratedEntity>
Chunk::takeGeneratedEntitiesMatching(const std::function<bool(const ChunkGeneratedEntity&)>& predicate) {
    std::vector<ChunkGeneratedEntity> taken;
    for (const ChunkGeneratedEntity& entity : generatedEntities_) {
        if (predicate(entity)) {
            taken.push_back(entity);
        }
    }
    if (taken.empty()) {
        return taken;
    }
    std::erase_if(generatedEntities_, predicate);
    dirty_ = true;
    return taken;
}

std::size_t Chunk::residentBytes() const {
    std::size_t bytes = kHeightmapCount * 16 * 16 * sizeof(std::int32_t);
    for (const ChunkSection& section : sections_) {
        bytes += section.residentBytes();
    }
    return bytes;
}

void Chunk::addStructureStart(structure::StructureStartRecord start) {
    if (structures_.add(std::move(start))) {
        dirty_ = true;
    }
}

std::int32_t Chunk::blockEntityKey(std::int32_t x, std::int32_t y, std::int32_t z) const {
    return ((y - bounds_.minY()) << 8) | ((z & 15) << 4) | (x & 15);
}

void Chunk::updateHeights(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    if (!heightsValid_) {
        return;
    }
    // parity: Heightmap.update. A block at or above the recorded height raises it; removing the one
    // *at* it is the only case that has to look downwards, and only within that column.
    const auto column = static_cast<std::size_t>((x & 15) + (z & 15) * 16);
    for (std::size_t i = 0; i < kHeightmapCount; ++i) {
        const auto type = static_cast<HeightmapType>(i);
        std::vector<std::int32_t>& map = heights_[i];
        const std::int32_t current = map[column];
        if (heightmapMatches(type, stateId)) {
            if (y >= current) {
                map[column] = y + 1;
            }
        } else if (y == current - 1) {
            std::int32_t scan = y - 1;
            while (scan >= bounds_.minY() && !heightmapMatches(type, blockStateId(x, scan, z))) {
                scan--;
            }
            map[column] = scan + 1;
        }
    }
}

void Chunk::rebuildHeights() const {
    // All four in one downward pass per column, skipping the sections that hold only air.
    constexpr std::uint32_t allPending = (1U << kHeightmapCount) - 1;
    for (std::vector<std::int32_t>& map : heights_) {
        std::ranges::fill(map, bounds_.minY());
    }
    for (std::int32_t z = 0; z < 16; ++z) {
        for (std::int32_t x = 0; x < 16; ++x) {
            const auto column = static_cast<std::size_t>(x + z * 16);
            std::uint32_t pending = allPending;
            std::int32_t y = bounds_.maxY();
            while (y >= bounds_.minY() && pending != 0) {
                const ChunkSection& section = sections_[static_cast<std::size_t>((y >> 4) - bounds_.minSection())];
                if (section.nonAirBlockCount() == 0) {
                    y = (y & ~15) - 1;
                    continue;
                }
                const std::int32_t stateId = section.blockStateId(x, y & 15, z);
                for (std::size_t i = 0; i < kHeightmapCount; ++i) {
                    if ((pending & (1U << i)) == 0) {
                        continue;
                    }
                    if (!heightmapMatches(static_cast<HeightmapType>(i), stateId)) {
                        continue;
                    }
                    heights_[i][column] = y + 1;
                    pending &= ~(1U << i);
                }
                y--;
            }
        }
    }
    heightsValid_ = true;
}

namespace {

/// Finds an existing deferred write for that block. The lists stay in insertion order because
/// delivery order is observable, so this is a scan rather than a map lookup; a feature defers a
/// handful of blocks, not thousands.
template <typename Write>
[[nodiscard]] Write* findWrite(std::vector<Write>& writes, std::int32_t x, std::int32_t y, std::int32_t z) {
    for (Write& write : writes) {
        if (write.x == x && write.y == y && write.z == z) {
            return &write;
        }
    }
    return nullptr;
}

} // namespace

void Chunk::markShapeUpdate(std::int32_t worldX, std::int32_t y, std::int32_t worldZ) {
    const ChunkShapeUpdate mark{.x = worldX, .y = y, .z = worldZ};
    if (std::ranges::find(shapeUpdates_, mark) == shapeUpdates_.end()) {
        shapeUpdates_.push_back(mark);
    }
}

std::int32_t Chunk::decorationBlockStateId(std::int32_t worldX, std::int32_t y, std::int32_t worldZ) const {
    if (y < bounds_.minY() || y > bounds_.maxY()) {
        return registry::blocks::air().stateId;
    }
    if ((worldX >> 4) == chunkX_ && (worldZ >> 4) == chunkZ_) {
        return blockStateId(worldX & 15, y, worldZ & 15);
    }
    for (const ChunkGeneratedBlockWrite& write : generatedBlockWrites_) {
        if (write.x == worldX && write.y == y && write.z == worldZ) {
            return write.stateId;
        }
    }
    if (externalDecorationState_) {
        if (const std::optional<std::int32_t> observed = externalDecorationState_(worldX, y, worldZ);
            observed.has_value()) {
            return *observed;
        }
    }
    return registry::blocks::air().stateId;
}

void Chunk::setDecorationBlockStateId(std::int32_t worldX, std::int32_t y, std::int32_t worldZ, std::int32_t stateId,
                                      std::optional<std::int32_t> expectedExternalStateId,
                                      bool replaceAnyExternalState) {
    if (y < bounds_.minY() || y > bounds_.maxY()) {
        return;
    }
    if ((worldX >> 4) == chunkX_ && (worldZ >> 4) == chunkZ_) {
        setBlockStateId(worldX & 15, y, worldZ & 15, stateId);
        return;
    }

    const std::int32_t expected = expectedExternalStateId.value_or(registry::blocks::air().stateId);
    ChunkGeneratedBlockWrite* previous = findWrite(generatedBlockWrites_, worldX, y, worldZ);
    // What is really out there beats what the caller assumed: a feature replacing stone rather than
    // air would otherwise defer with an air guard and have the write refused on delivery.
    std::optional<std::int32_t> observed;
    if (externalDecorationState_) {
        observed = externalDecorationState_(worldX, y, worldZ);
    }
    const std::int32_t oldStateId = previous != nullptr ? previous->stateId : observed.value_or(expected);
    if (oldStateId == stateId) {
        if (blockWriteObserver_) {
            blockWriteObserver_(worldX - chunkX_ * 16, y, worldZ - chunkZ_ * 16, oldStateId, stateId);
        }
        return;
    }

    const ChunkGeneratedBlockWrite write{
        .x = worldX,
        .y = y,
        .z = worldZ,
        .expectedStateId = previous != nullptr ? previous->expectedStateId : observed.value_or(expected),
        .stateId = stateId,
        .replaceAnyState = (previous != nullptr && previous->replaceAnyState) || replaceAnyExternalState,
        .policy = generatedBlockWritePolicy_,
    };
    if (previous != nullptr) {
        *previous = write;
    } else {
        generatedBlockWrites_.push_back(write);
    }
    if (blockWriteObserver_) {
        blockWriteObserver_(worldX - chunkX_ * 16, y, worldZ - chunkZ_ * 16, oldStateId, stateId);
    }
}

std::vector<ChunkGeneratedBlockWrite> Chunk::takeGeneratedBlockWrites() {
    return std::exchange(generatedBlockWrites_, {});
}

void Chunk::setDecorationBlockEntity(std::int32_t worldX, std::int32_t y, std::int32_t worldZ,
                                     std::int32_t expectedStateId, wire::common::Identifier type,
                                     wire::nbt::NbtCompound data) {
    if (y < bounds_.minY() || y > bounds_.maxY()) {
        return;
    }
    if ((worldX >> 4) == chunkX_ && (worldZ >> 4) == chunkZ_) {
        setBlockEntity(ChunkBlockEntity{worldX & 15, y, worldZ & 15, std::move(type), std::move(data)});
        return;
    }
    ChunkGeneratedBlockEntityWrite write{.x = worldX,
                                         .y = y,
                                         .z = worldZ,
                                         .expectedStateId = expectedStateId,
                                         .type = std::move(type),
                                         .data = std::move(data)};
    if (ChunkGeneratedBlockEntityWrite* previous = findWrite(generatedBlockEntityWrites_, worldX, y, worldZ);
        previous != nullptr) {
        *previous = std::move(write);
    } else {
        generatedBlockEntityWrites_.push_back(std::move(write));
    }
}

std::vector<ChunkGeneratedBlockEntityWrite> Chunk::takeGeneratedBlockEntityWrites() {
    return std::exchange(generatedBlockEntityWrites_, {});
}

void Chunk::scheduleDecorationBlockTick(std::int32_t worldX, std::int32_t y, std::int32_t worldZ,
                                        std::int32_t expectedStateId, wire::common::Identifier type, std::int32_t delay,
                                        std::int32_t priority) {
    if (y < bounds_.minY() || y > bounds_.maxY()) {
        return;
    }
    if ((worldX >> 4) == chunkX_ && (worldZ >> 4) == chunkZ_) {
        if (blockStateId(worldX & 15, y, worldZ & 15) == expectedStateId) {
            scheduleBlockTick(ChunkScheduledTick{std::move(type), worldX & 15, y, worldZ & 15, delay, priority});
        }
        return;
    }
    ChunkGeneratedBlockTick tick{.x = worldX,
                                 .y = y,
                                 .z = worldZ,
                                 .expectedStateId = expectedStateId,
                                 .type = std::move(type),
                                 .delay = delay,
                                 .priority = priority};
    if (ChunkGeneratedBlockTick* previous = findWrite(generatedBlockTicks_, worldX, y, worldZ); previous != nullptr) {
        *previous = std::move(tick);
    } else {
        generatedBlockTicks_.push_back(std::move(tick));
    }
}

std::vector<ChunkGeneratedBlockTick> Chunk::takeGeneratedBlockTicks() {
    return std::exchange(generatedBlockTicks_, {});
}

Chunk::BlockWriteObserverScope::BlockWriteObserverScope(Chunk& chunk, Observer observer) : chunk_(&chunk) {
    if (chunk_->blockWriteObserver_) {
        throw std::logic_error{"nested block write observers are not supported"};
    }
    chunk_->blockWriteObserver_ = std::move(observer);
}

Chunk::BlockWriteObserverScope::~BlockWriteObserverScope() { chunk_->blockWriteObserver_ = nullptr; }

Chunk::ExternalStateScope::ExternalStateScope(Chunk& chunk, Reader reader)
    : chunk_(&chunk), previous_(std::move(chunk.externalDecorationState_)) {
    chunk_->externalDecorationState_ = std::move(reader);
}

Chunk::ExternalStateScope::~ExternalStateScope() { chunk_->externalDecorationState_ = std::move(previous_); }

Chunk::WritePolicyScope::WritePolicyScope(Chunk& chunk, GeneratedBlockWritePolicy policy)
    : chunk_(&chunk), previous_(chunk.generatedBlockWritePolicy_) {
    chunk_->generatedBlockWritePolicy_ = policy;
}

Chunk::WritePolicyScope::~WritePolicyScope() { chunk_->generatedBlockWritePolicy_ = previous_; }

} // namespace basalt::server::world
