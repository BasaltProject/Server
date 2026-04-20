#include <basalt/server/runtime/world/RegionStore.hpp>
#include <basalt/server/world/region/ChunkNbt.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <chrono>
#include <cstdint>
#include <map>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

using server::world::Chunk;
using server::world::region::LinearRegion;

[[nodiscard]] std::int64_t millisSince(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
}

[[nodiscard]] std::string parentOf(const std::string& path) {
    const std::size_t at = path.find_last_of('/');
    return at == std::string::npos ? std::string{"."} : path.substr(0, at);
}

} // namespace

RegionStore::RegionStore(std::string worldName, std::string worldsRoot, server::world::ChunkHeight verticalBounds,
                         std::size_t capacity, std::int64_t byteBudget)
    : worldRoot_(std::move(worldsRoot) + "/" + std::move(worldName)), verticalBounds_(verticalBounds),
      capacity_(capacity), byteBudget_(byteBudget) {}

std::int64_t RegionStore::key(std::int32_t regionX, std::int32_t regionZ) {
    return (static_cast<std::int64_t>(regionX) & 0xFFFFFFFFLL) | (static_cast<std::int64_t>(regionZ) << 32);
}

std::int32_t RegionStore::regionXOf(std::int64_t regionKey) { return static_cast<std::int32_t>(regionKey); }

std::int32_t RegionStore::regionZOf(std::int64_t regionKey) { return static_cast<std::int32_t>(regionKey >> 32); }

std::string RegionStore::regionPath(std::int32_t regionX, std::int32_t regionZ) const {
    return worldRoot_ + "/region/r." + std::to_string(regionX) + "." + std::to_string(regionZ) + ".linear";
}

std::size_t RegionStore::cachedRegionCount() const {
    const std::lock_guard<std::mutex> lock{mutex_};
    return cached_.size();
}

std::int64_t RegionStore::cachedRegionBytes() const {
    const std::lock_guard<std::mutex> lock{mutex_};
    return cachedBytesLocked();
}

std::int64_t RegionStore::cachedBytesLocked() const {
    std::int64_t total = 0;
    for (const Entry& entry : cached_) {
        total += entry.region.residentBytes();
    }
    return total;
}

LinearRegion* RegionStore::touch(std::int64_t regionKey) {
    const auto found = byKey_.find(regionKey);
    if (found == byKey_.end()) {
        return nullptr;
    }
    cached_.splice(cached_.end(), cached_, found->second);
    return &found->second->region;
}

LinearRegion& RegionStore::store(std::int64_t regionKey, LinearRegion region) {
    cached_.push_back(Entry{.key = regionKey, .region = std::move(region)});
    byKey_[regionKey] = std::prev(cached_.end());
    if (cached_.size() <= capacity_ && cachedBytesLocked() <= byteBudget_) {
        return cached_.back().region;
    }
    for (auto at = cached_.begin();
         at != cached_.end() && (cached_.size() > capacity_ || cachedBytesLocked() > byteBudget_);) {
        if (at->key == regionKey || dirtyRegions_.contains(at->key)) {
            ++at;
            continue;
        }
        byKey_.erase(at->key);
        at = cached_.erase(at);
    }
    return byKey_.at(regionKey)->region;
}

LinearRegion* RegionStore::region(std::int32_t regionX, std::int32_t regionZ) {
    const std::int64_t regionKey = key(regionX, regionZ);
    if (LinearRegion* cached = touch(regionKey); cached != nullptr) {
        return cached;
    }
    const std::optional<std::vector<std::uint8_t>> bytes = wire::io::fileIO::readBytes(regionPath(regionX, regionZ));
    if (!bytes.has_value()) {
        return nullptr;
    }
    std::expected<LinearRegion, server::world::region::LinearError> decoded = LinearRegion::read(*bytes);
    if (!decoded.has_value()) {
        return nullptr;
    }
    return &store(regionKey, *std::move(decoded));
}

std::optional<Chunk> RegionStore::loadChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    const std::int32_t regionX = chunkX >> 5;
    const std::int32_t regionZ = chunkZ >> 5;
    const std::int64_t regionKey = key(regionX, regionZ);

    bool missing = false;
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        missing = touch(regionKey) == nullptr;
    }
    if (missing) {
        const std::optional<std::vector<std::uint8_t>> bytes =
            wire::io::fileIO::readBytes(regionPath(regionX, regionZ));
        if (!bytes.has_value()) {
            return std::nullopt;
        }
        std::expected<LinearRegion, server::world::region::LinearError> read = LinearRegion::read(*bytes);
        if (!read.has_value()) {
            return std::nullopt;
        }
        const std::lock_guard<std::mutex> lock{mutex_};
        if (touch(regionKey) == nullptr) {
            store(regionKey, *std::move(read));
        }
    }

    std::vector<std::uint8_t> chunkBytes;
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        LinearRegion* held = touch(regionKey);
        if (held == nullptr) {
            return std::nullopt;
        }
        const std::span<const std::uint8_t> blob = held->chunkAt(chunkX & 31, chunkZ & 31);
        if (blob.empty()) {
            return std::nullopt;
        }
        chunkBytes.assign(blob.begin(), blob.end());
    }

    // Decoding happens outside the lock: it is the expensive half, and it touches nothing shared.
    const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> tag = wire::nbt::nbtIO::read(chunkBytes);
    if (!tag.has_value()) {
        return std::nullopt;
    }
    const wire::nbt::NbtCompound* root = tag->tag.get<wire::nbt::NbtCompound>();
    if (root == nullptr) {
        return std::nullopt;
    }
    std::expected<Chunk, server::world::region::ChunkNbtError> chunk =
        server::world::region::chunkNbt::read(*root, verticalBounds_);
    if (!chunk.has_value()) {
        return std::nullopt;
    }
    return std::move(*chunk);
}

std::size_t RegionStore::saveChunks(std::span<Chunk* const> chunks, std::int32_t timestamp) {
    if (chunks.empty()) {
        return 0;
    }
    const std::lock_guard<std::mutex> writeLock{writeMutex_};

    std::map<std::int64_t, std::vector<Chunk*>> byRegion;
    for (Chunk* chunk : chunks) {
        byRegion[key(chunk->chunkX() >> 5, chunk->chunkZ() >> 5)].push_back(chunk);
    }

    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::int64_t encodeTotal = 0;
    for (const auto& [regionKey, regionChunks] : byRegion) {
        const std::chrono::steady_clock::time_point encodeStart = std::chrono::steady_clock::now();
        std::vector<std::pair<Chunk*, std::vector<std::uint8_t>>> encoded;
        encoded.reserve(regionChunks.size());
        for (Chunk* chunk : regionChunks) {
            encoded.emplace_back(chunk, wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(
                                            server::world::region::chunkNbt::write(*chunk))));
        }
        encodeTotal += millisSince(encodeStart);

        const std::lock_guard<std::mutex> lock{mutex_};
        LinearRegion* target = region(regionXOf(regionKey), regionZOf(regionKey));
        if (target == nullptr) {
            target = &store(regionKey, LinearRegion{});
        }
        for (auto& [chunk, bytes] : encoded) {
            target->setChunk(chunk->chunkX() & 31, chunk->chunkZ() & 31, std::move(bytes), timestamp);
        }
        dirtyRegions_.insert(regionKey);
    }
    encodeMillis_ = encodeTotal;
    const std::size_t touched = flushDirty();
    writeMillis_ = millisSince(start) - encodeTotal;
    for (Chunk* chunk : chunks) {
        chunk->markClean();
    }
    return touched;
}

std::size_t RegionStore::flush() {
    const std::lock_guard<std::mutex> writeLock{writeMutex_};
    return flushDirty();
}

std::size_t RegionStore::flushDirty() {
    std::vector<std::pair<std::int64_t, LinearRegion*>> toWrite;
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        for (const std::int64_t regionKey : dirtyRegions_) {
            if (const auto found = byKey_.find(regionKey); found != byKey_.end()) {
                toWrite.emplace_back(regionKey, &found->second->region);
            }
        }
        dirtyRegions_.clear();
    }

    std::size_t written = 0;
    for (const auto& [regionKey, held] : toWrite) {
        const std::string path = regionPath(regionXOf(regionKey), regionZOf(regionKey));
        wire::io::fileIO::createDirectories(parentOf(path));
        if (held->writeTo(path)) {
            ++written;
        }
    }
    return written;
}

} // namespace basalt::server::runtime::world
