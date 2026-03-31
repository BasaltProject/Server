#include <basalt/server/runtime/world/poi/PoiManager.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace basalt::server::runtime::world::poi {
namespace {

constexpr std::int32_t kNoVillage = PoiManager::kMaxVillageDistance + 1;

/// parity: `HashMap`'s default table and its three-quarter load factor.
constexpr std::uint32_t kInitialBuckets = 16;

[[nodiscard]] std::int32_t floorDiv(std::int32_t value, std::int32_t divisor) {
    std::int32_t quotient = value / divisor;
    if ((value % divisor != 0) && ((value < 0) != (divisor < 0))) {
        --quotient;
    }
    return quotient;
}

/// parity: `Vec3i.hashCode` spread the way `HashMap.hash` spreads it.
[[nodiscard]] std::uint32_t spreadHash(const BlockPos& pos) {
    const auto hash = static_cast<std::uint32_t>(
        ((static_cast<std::uint32_t>(pos.y) + (static_cast<std::uint32_t>(pos.z) * 31U)) * 31U) +
        static_cast<std::uint32_t>(pos.x));
    return hash ^ (hash >> 16U);
}

[[nodiscard]] std::uint32_t bucketsFor(std::uint16_t peak) {
    std::uint32_t buckets = kInitialBuckets;
    while (static_cast<std::uint32_t>(peak) * 4U > buckets * 3U) {
        buckets *= 2U;
    }
    return buckets;
}

[[nodiscard]] bool passes(const PoiRecord& record, Occupancy occupancy) {
    switch (occupancy) {
    case Occupancy::kHasSpace:
        return record.hasSpace();
    case Occupancy::kIsOccupied:
        return record.isOccupied();
    case Occupancy::kAny:
        return true;
    }
    return true;
}

[[nodiscard]] double distSqr(const BlockPos& one, const BlockPos& other) {
    const auto dx = static_cast<double>(one.x - other.x);
    const auto dy = static_cast<double>(one.y - other.y);
    const auto dz = static_cast<double>(one.z - other.z);
    return (dx * dx) + (dy * dy) + (dz * dz);
}

[[nodiscard]] std::int32_t chebyshev(std::int64_t one, std::int64_t other);

[[nodiscard]] std::int32_t unpackSectionX(std::int64_t key) { return static_cast<std::int32_t>(key >> 42); }

[[nodiscard]] std::int32_t unpackSectionY(std::int64_t key) { return static_cast<std::int32_t>(key << 44 >> 44); }

[[nodiscard]] std::int32_t unpackSectionZ(std::int64_t key) { return static_cast<std::int32_t>(key << 22 >> 42); }

std::int32_t chebyshev(std::int64_t one, std::int64_t other) {
    return std::max({std::abs(unpackSectionX(one) - unpackSectionX(other)),
                     std::abs(unpackSectionY(one) - unpackSectionY(other)),
                     std::abs(unpackSectionZ(one) - unpackSectionZ(other))});
}

} // namespace

PoiManager::PoiManager(server::world::ChunkHeight bounds) : bounds_(bounds) {}

void PoiManager::configure(std::string directory, server::world::ChunkHeight bounds) {
    const std::lock_guard lock{mutex_};
    directory_ = std::move(directory);
    bounds_ = bounds;
}

std::int64_t PoiManager::chunkKey(std::int32_t chunkX, std::int32_t chunkZ) {
    return (static_cast<std::int64_t>(chunkX) & 0xFFFFFFFFLL) | (static_cast<std::int64_t>(chunkZ) << 32);
}

std::int64_t PoiManager::sectionKey(std::int32_t x, std::int32_t y, std::int32_t z) {
    // parity: `SectionPos.asLong`.
    return ((static_cast<std::int64_t>(x) & 0x3FFFFFLL) << 42) | (static_cast<std::int64_t>(y) & 0xFFFFFLL) |
           ((static_cast<std::int64_t>(z) & 0x3FFFFFLL) << 20);
}

const PoiManager::ChunkPoi* PoiManager::findChunkLocked(std::int32_t chunkX, std::int32_t chunkZ) {
    ensureRegionLocked(chunkX >> 5, chunkZ >> 5);
    const auto found = chunks_.find(chunkKey(chunkX, chunkZ));
    return found == chunks_.end() ? nullptr : &found->second;
}

PoiManager::ChunkPoi& PoiManager::chunkLocked(std::int32_t chunkX, std::int32_t chunkZ) {
    ensureRegionLocked(chunkX >> 5, chunkZ >> 5);
    ChunkPoi& chunk = chunks_[chunkKey(chunkX, chunkZ)];
    if (chunk.sections.empty()) {
        chunk.sections.resize(static_cast<std::size_t>(bounds_.sectionCount()));
    }
    return chunk;
}

PoiManager::Section* PoiManager::sectionLocked(std::int32_t x, std::int32_t y, std::int32_t z, bool create) {
    const std::int32_t index = (y >> 4) - bounds_.minSection();
    if (index < 0 || index >= bounds_.sectionCount()) {
        return nullptr;
    }
    if (!create) {
        const ChunkPoi* chunk = findChunkLocked(x >> 4, z >> 4);
        if (chunk == nullptr) {
            return nullptr;
        }
        const std::optional<Section>& section = chunk->sections[static_cast<std::size_t>(index)];
        return section.has_value() ? const_cast<Section*>(&*section) : nullptr;
    }
    std::optional<Section>& section = chunkLocked(x >> 4, z >> 4).sections[static_cast<std::size_t>(index)];
    if (!section.has_value()) {
        section.emplace();
    }
    return &*section;
}

void PoiManager::addLocked(Section& section, const BlockPos& pos, PoiType type, std::int32_t freeTickets) {
    const auto existing =
        std::ranges::find_if(section.records, [&pos](const PoiRecord& record) { return record.pos == pos; });
    if (existing != section.records.end()) {
        // parity: `PoiSection.add` keeps a record of the same type and replaces one of another.
        if (existing->type == type) {
            return;
        }
        section.records.erase(existing);
    }
    section.records.push_back(PoiRecord{.pos = pos, .type = type, .freeTickets = freeTickets});
    const auto count = static_cast<std::uint16_t>(
        std::ranges::count_if(section.records, [type](const PoiRecord& record) { return record.type == type; }));
    std::uint16_t& peak = section.peak[static_cast<std::size_t>(type)];
    peak = std::max(peak, count);
}

void PoiManager::removeLocked(Section& section, const BlockPos& pos) {
    std::erase_if(section.records, [&pos](const PoiRecord& record) { return record.pos == pos; });
}

void PoiManager::changedLocked(const BlockPos& pos) {
    dirtyRegions_.insert(chunkKey(pos.x >> 9, pos.z >> 9));
    refreshCenterLocked(pos.x >> 4, pos.y >> 4, pos.z >> 4);
}

void PoiManager::refreshCenterLocked(std::int32_t sectionX, std::int32_t sectionY, std::int32_t sectionZ) {
    const std::int64_t key = sectionKey(sectionX, sectionY, sectionZ);
    const Section* section = sectionLocked(sectionX << 4, sectionY << 4, sectionZ << 4, false);
    // parity: `isVillageCenter`, any occupied village point in the section.
    const bool center = section != nullptr && std::ranges::any_of(section->records, [](const PoiRecord& record) {
                            return matches(poiTypes::kVillage, record.type) && record.isOccupied();
                        });
    if (center == centers_.contains(key)) {
        return;
    }
    if (center) {
        centers_.insert(key);
        for (std::int32_t dx = -kMaxVillageDistance; dx <= kMaxVillageDistance; ++dx) {
            for (std::int32_t dy = -kMaxVillageDistance; dy <= kMaxVillageDistance; ++dy) {
                for (std::int32_t dz = -kMaxVillageDistance; dz <= kMaxVillageDistance; ++dz) {
                    const auto distance =
                        static_cast<std::uint8_t>(std::max({std::abs(dx), std::abs(dy), std::abs(dz)}));
                    const std::int64_t other = sectionKey(sectionX + dx, sectionY + dy, sectionZ + dz);
                    const auto found = levels_.find(other);
                    if (found == levels_.end() || found->second > distance) {
                        levels_[other] = distance;
                    }
                }
            }
        }
        return;
    }
    centers_.erase(key);
    std::vector<std::int64_t> near;
    for (const std::int64_t other : centers_) {
        if (chebyshev(other, key) <= 2 * kMaxVillageDistance) {
            near.push_back(other);
        }
    }
    for (std::int32_t dx = -kMaxVillageDistance; dx <= kMaxVillageDistance; ++dx) {
        for (std::int32_t dy = -kMaxVillageDistance; dy <= kMaxVillageDistance; ++dy) {
            for (std::int32_t dz = -kMaxVillageDistance; dz <= kMaxVillageDistance; ++dz) {
                const std::int64_t target = sectionKey(sectionX + dx, sectionY + dy, sectionZ + dz);
                std::int32_t best = kNoVillage;
                for (const std::int64_t other : near) {
                    best = std::min(best, chebyshev(other, target));
                }
                if (best > kMaxVillageDistance) {
                    levels_.erase(target);
                } else {
                    levels_[target] = static_cast<std::uint8_t>(best);
                }
            }
        }
    }
}

void PoiManager::noteBlockChange(const BlockPos& pos, std::int32_t oldStateId, std::int32_t newStateId) {
    const PoiType was = poiTypes::forState(oldStateId);
    const PoiType now = poiTypes::forState(newStateId);
    if (was == now) {
        return;
    }
    const std::lock_guard lock{mutex_};
    if (was != PoiType::kNone) {
        if (Section* section = sectionLocked(pos.x, pos.y, pos.z, false); section != nullptr) {
            removeLocked(*section, pos);
        }
    }
    if (now != PoiType::kNone) {
        if (Section* section = sectionLocked(pos.x, pos.y, pos.z, true); section != nullptr) {
            addLocked(*section, pos, now, poiTypes::maxTickets(now));
        }
    }
    changedLocked(pos);
}

void PoiManager::noteBlockState(const BlockPos& pos, std::int32_t stateId) {
    const PoiType now = poiTypes::forState(stateId);
    const std::lock_guard lock{mutex_};
    Section* section = sectionLocked(pos.x, pos.y, pos.z, now != PoiType::kNone);
    if (section == nullptr) {
        return;
    }
    const auto existing =
        std::ranges::find_if(section->records, [&pos](const PoiRecord& record) { return record.pos == pos; });
    if (existing != section->records.end() && existing->type == now) {
        return;
    }
    if (existing == section->records.end() && now == PoiType::kNone) {
        return;
    }
    removeLocked(*section, pos);
    if (now != PoiType::kNone) {
        addLocked(*section, pos, now, poiTypes::maxTickets(now));
    }
    changedLocked(pos);
}

void PoiManager::chunkInstalled(const server::world::Chunk& chunk) {
    const std::lock_guard lock{mutex_};
    const std::int32_t baseX = chunk.chunkX() << 4;
    const std::int32_t baseZ = chunk.chunkZ() << 4;
    const std::size_t count = std::min(chunk.sectionCount(), static_cast<std::size_t>(bounds_.sectionCount()));
    for (std::size_t index = 0; index < count; ++index) {
        const ChunkPoi* known = findChunkLocked(chunk.chunkX(), chunk.chunkZ());
        const bool present = known != nullptr && known->sections[index].has_value();
        if (present && known->sections[index]->valid) {
            continue;
        }
        const server::world::ChunkSection& blocks = chunk.section(index);
        // parity: `mayHavePoi`, the palette asked before a single block is.
        const bool mayHave =
            blocks.mayContain([](std::int32_t stateId) { return poiTypes::forState(stateId) != PoiType::kNone; });
        if (!mayHave && !present) {
            continue;
        }
        std::optional<Section>& section = chunkLocked(chunk.chunkX(), chunk.chunkZ()).sections[index];
        const std::int32_t baseY = (bounds_.minSection() + static_cast<std::int32_t>(index)) << 4;
        // parity: `PoiSection.refresh`, which keeps what an old record at the same spot knew.
        std::vector<PoiRecord> old;
        if (section.has_value()) {
            old = std::move(section->records);
        }
        section.emplace();
        if (mayHave) {
            for (std::int32_t localY = 0; localY < 16; ++localY) {
                for (std::int32_t localZ = 0; localZ < 16; ++localZ) {
                    for (std::int32_t localX = 0; localX < 16; ++localX) {
                        const PoiType type = poiTypes::forState(blocks.blockStateId(localX, localY, localZ));
                        if (type == PoiType::kNone) {
                            continue;
                        }
                        const BlockPos pos{baseX + localX, baseY + localY, baseZ + localZ};
                        const auto kept =
                            std::ranges::find_if(old, [&pos](const PoiRecord& record) { return record.pos == pos; });
                        addLocked(*section, pos, kept != old.end() ? kept->type : type,
                                  kept != old.end() ? kept->freeTickets : poiTypes::maxTickets(type));
                    }
                }
            }
        }
        const std::int32_t sectionY = bounds_.minSection() + static_cast<std::int32_t>(index);
        dirtyRegions_.insert(chunkKey(chunk.chunkX() >> 5, chunk.chunkZ() >> 5));
        refreshCenterLocked(chunk.chunkX(), sectionY, chunk.chunkZ());
    }
}

std::optional<PoiRecord> PoiManager::record(const BlockPos& pos) {
    const std::lock_guard lock{mutex_};
    const Section* section = sectionLocked(pos.x, pos.y, pos.z, false);
    if (section == nullptr) {
        return std::nullopt;
    }
    const auto found =
        std::ranges::find_if(section->records, [&pos](const PoiRecord& record) { return record.pos == pos; });
    if (found == section->records.end()) {
        return std::nullopt;
    }
    return *found;
}

std::optional<PoiType> PoiManager::type(const BlockPos& pos) {
    const std::optional<PoiRecord> found = record(pos);
    if (!found.has_value()) {
        return std::nullopt;
    }
    return found->type;
}

bool PoiManager::exists(const BlockPos& pos, PoiMask mask) {
    const std::optional<PoiType> found = type(pos);
    return found.has_value() && matches(mask, *found);
}

void PoiManager::walkLocked(PoiMask mask, const BlockPos& center, std::int32_t radius, Occupancy occupancy, bool round,
                            std::vector<PoiRecord>& out) {
    out.clear();
    const std::int32_t chunkRadius = floorDiv(radius, 16) + 1;
    const std::int32_t centerChunkX = center.x >> 4;
    const std::int32_t centerChunkZ = center.z >> 4;
    const double radiusSqr = static_cast<double>(radius) * static_cast<double>(radius);
    std::vector<std::pair<std::uint32_t, std::size_t>> order;
    // parity: `ChunkPos.rangeClosed`, x fastest.
    for (std::int32_t chunkZ = centerChunkZ - chunkRadius; chunkZ <= centerChunkZ + chunkRadius; ++chunkZ) {
        for (std::int32_t chunkX = centerChunkX - chunkRadius; chunkX <= centerChunkX + chunkRadius; ++chunkX) {
            const ChunkPoi* chunk = findChunkLocked(chunkX, chunkZ);
            if (chunk == nullptr) {
                continue;
            }
            for (const std::optional<Section>& section : chunk->sections) {
                if (!section.has_value() || section->records.empty()) {
                    continue;
                }
                for (std::size_t typeIndex = 1; typeIndex < static_cast<std::size_t>(PoiType::kCount); ++typeIndex) {
                    const auto type = static_cast<PoiType>(typeIndex);
                    if (!matches(mask, type)) {
                        continue;
                    }
                    const std::uint32_t buckets = bucketsFor(section->peak[typeIndex]);
                    order.clear();
                    for (std::size_t index = 0; index < section->records.size(); ++index) {
                        if (section->records[index].type == type) {
                            order.emplace_back(spreadHash(section->records[index].pos) & (buckets - 1U), index);
                        }
                    }
                    std::ranges::stable_sort(order, {}, &std::pair<std::uint32_t, std::size_t>::first);
                    for (const auto& [bucket, index] : order) {
                        const PoiRecord& record = section->records[index];
                        if (!passes(record, occupancy) || std::abs(record.pos.x - center.x) > radius ||
                            std::abs(record.pos.z - center.z) > radius) {
                            continue;
                        }
                        if (round && distSqr(record.pos, center) > radiusSqr) {
                            continue;
                        }
                        out.push_back(record);
                    }
                }
            }
        }
    }
}

void PoiManager::inRange(PoiMask mask, const BlockPos& center, std::int32_t radius, Occupancy occupancy,
                         std::vector<PoiRecord>& out) {
    const std::lock_guard lock{mutex_};
    walkLocked(mask, center, radius, occupancy, true, out);
}

void PoiManager::inSquare(PoiMask mask, const BlockPos& center, std::int32_t radius, Occupancy occupancy,
                          std::vector<PoiRecord>& out) {
    const std::lock_guard lock{mutex_};
    walkLocked(mask, center, radius, occupancy, false, out);
}

std::int64_t PoiManager::countInRange(PoiMask mask, const BlockPos& center, std::int32_t radius, Occupancy occupancy) {
    std::vector<PoiRecord> found;
    inRange(mask, center, radius, occupancy, found);
    return static_cast<std::int64_t>(found.size());
}

std::optional<BlockPos> PoiManager::findClosest(PoiMask mask, const BlockPos& center, std::int32_t radius,
                                                Occupancy occupancy) {
    std::vector<PoiRecord> found;
    inRange(mask, center, radius, occupancy, found);
    std::optional<BlockPos> best;
    double bestDistance = std::numeric_limits<double>::max();
    for (const PoiRecord& record : found) {
        const double distance = distSqr(record.pos, center);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = record.pos;
        }
    }
    return best;
}

bool PoiManager::acquire(const BlockPos& pos) {
    const std::lock_guard lock{mutex_};
    Section* section = sectionLocked(pos.x, pos.y, pos.z, false);
    if (section == nullptr) {
        return false;
    }
    const auto found =
        std::ranges::find_if(section->records, [&pos](const PoiRecord& record) { return record.pos == pos; });
    if (found == section->records.end() || found->freeTickets <= 0) {
        return false;
    }
    --found->freeTickets;
    changedLocked(pos);
    return true;
}

bool PoiManager::release(const BlockPos& pos) {
    const std::lock_guard lock{mutex_};
    Section* section = sectionLocked(pos.x, pos.y, pos.z, false);
    if (section == nullptr) {
        return false;
    }
    const auto found =
        std::ranges::find_if(section->records, [&pos](const PoiRecord& record) { return record.pos == pos; });
    if (found == section->records.end()) {
        return false;
    }
    bool released = false;
    if (found->freeTickets < poiTypes::maxTickets(found->type)) {
        ++found->freeTickets;
        released = true;
    }
    changedLocked(pos);
    return released;
}

std::int32_t PoiManager::sectionsToVillage(std::int32_t sectionX, std::int32_t sectionY, std::int32_t sectionZ) {
    const std::lock_guard lock{mutex_};
    // The sections around the question have to be known before the answer is.
    for (std::int32_t regionZ = (sectionZ - kMaxVillageDistance) >> 5; regionZ <= (sectionZ + kMaxVillageDistance) >> 5;
         ++regionZ) {
        for (std::int32_t regionX = (sectionX - kMaxVillageDistance) >> 5;
             regionX <= (sectionX + kMaxVillageDistance) >> 5; ++regionX) {
            ensureRegionLocked(regionX, regionZ);
        }
    }
    const auto found = levels_.find(sectionKey(sectionX, sectionY, sectionZ));
    return found == levels_.end() ? kNoVillage : found->second;
}

bool PoiManager::isCloseToVillage(const BlockPos& pos, std::int32_t sectionDistance) {
    if (sectionDistance > kMaxVillageDistance) {
        return false;
    }
    return sectionsToVillage(pos.x >> 4, pos.y >> 4, pos.z >> 4) <= sectionDistance;
}

} // namespace basalt::server::runtime::world::poi
