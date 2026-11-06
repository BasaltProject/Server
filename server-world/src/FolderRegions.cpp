#include <basalt/metamorph/region/LinearRegion.hpp>
#include <basalt/server/world/FolderRegions.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <filesystem>
#include <map>
#include <mutex>

namespace basalt::server::world::folderRegions {
namespace {
using metamorph::region::LinearRegion;

struct Held {
    LinearRegion region;
    bool dirty = false;
};

struct Folder {
    std::map<std::pair<std::int32_t, std::int32_t>, Held> regions;
};

std::mutex gMutex;
std::map<std::string, Folder> gFolders;

std::int32_t floorDiv32(std::int32_t value) { return value >> 5; }

std::string pathOf(const std::string& folder, std::int32_t regionX, std::int32_t regionZ) {
    return folder + "/r." + std::to_string(regionX) + "." + std::to_string(regionZ) + ".linear";
}

Held* held(const std::string& folder, std::int32_t regionX, std::int32_t regionZ, bool create) {
    Folder& entry = gFolders[folder];
    const auto key = std::make_pair(regionX, regionZ);
    if (auto found = entry.regions.find(key); found != entry.regions.end()) {
        return &found->second;
    }
    Held loaded;
    if (const auto bytes = wire::io::fileIO::readBytes(pathOf(folder, regionX, regionZ)); bytes.has_value()) {
        auto parsed = LinearRegion::read(*bytes);
        if (!parsed.has_value()) {
            return nullptr;
        }
        loaded.region = std::move(*parsed);
    } else if (!create) {
        return nullptr;
    }
    return &entry.regions.emplace(key, std::move(loaded)).first->second;
}

bool flushLocked(const std::string& folder) {
    bool ok = true;
    auto found = gFolders.find(folder);
    if (found == gFolders.end()) {
        return true;
    }
    std::filesystem::create_directories(folder);
    for (auto& [key, entry] : found->second.regions) {
        if (entry.dirty) {
            ok = entry.region.writeTo(pathOf(folder, key.first, key.second)) && ok;
            entry.dirty = false;
        }
    }
    return ok;
}
} // namespace

std::optional<std::vector<std::uint8_t>> read(const std::string& folder, std::int32_t chunkX, std::int32_t chunkZ) {
    const std::lock_guard lock(gMutex);
    Held* region = held(folder, floorDiv32(chunkX), floorDiv32(chunkZ), false);
    if (region == nullptr || !region->region.hasChunkAt(chunkX & 31, chunkZ & 31)) {
        return std::nullopt;
    }
    const auto bytes = region->region.chunkAt(chunkX & 31, chunkZ & 31);
    return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
}

void write(const std::string& folder, std::int32_t chunkX, std::int32_t chunkZ, std::vector<std::uint8_t> data,
           std::int32_t timestamp) {
    const std::lock_guard lock(gMutex);
    Held* region = held(folder, floorDiv32(chunkX), floorDiv32(chunkZ), true);
    if (region != nullptr) {
        region->region.setChunk(chunkX & 31, chunkZ & 31, std::move(data), timestamp);
        region->dirty = true;
    }
}

std::int32_t timestamp(const std::string& folder, std::int32_t chunkX, std::int32_t chunkZ) {
    const std::lock_guard lock(gMutex);
    Held* region = held(folder, floorDiv32(chunkX), floorDiv32(chunkZ), false);
    return region == nullptr ? 0 : region->region.timestampAt(chunkX & 31, chunkZ & 31);
}

bool flush(const std::string& folder) {
    const std::lock_guard lock(gMutex);
    return flushLocked(folder);
}

bool release(const std::string& folder) {
    const std::lock_guard lock(gMutex);
    const bool ok = flushLocked(folder);
    gFolders.erase(folder);
    return ok;
}

} // namespace basalt::server::world::folderRegions
