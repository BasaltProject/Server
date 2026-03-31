#pragma once

#include <basalt/server/runtime/world/maps/MapData.hpp>

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world::maps {

/// Every map on the server and the counter that numbers new ones. Shared across world threads, so
/// every call holds `lock()`.
/// parity: kept in the server's data storage, not a level's: `maps/<id>.dat` and `maps/last_id.dat`
/// beside `worlds/`, written uncompressed; gzipped vanilla files are read too.
class MapStorage {
public:
    static constexpr std::string_view kDefaultDirectory = "maps";

    explicit MapStorage(std::string directory = std::string{kDefaultDirectory}) : directory_(std::move(directory)) {}

    [[nodiscard]] std::unique_lock<std::mutex> lock() { return std::unique_lock{mutex_}; }

    /// The map with this id, read from disk the first time it is asked for, or null.
    [[nodiscard]] MapData* get(std::int32_t id);

    void set(std::int32_t id, MapData data);

    /// Every map read so far, by id.
    template <typename Visit>
    void forEachLoaded(Visit&& visit) {
        for (auto& [id, data] : maps_) {
            if (data.has_value()) {
                visit(id, *data);
            }
        }
    }

    /// parity: `MapIndex.getNextMapId`.
    [[nodiscard]] std::int32_t nextId();

    /// Structure starts an explorer map already points at, by level, structure and start chunk.
    ///
    /// parity: `StructureStart.references`, which vanilla keeps in the chunk. Kept here instead, so
    /// a world imported from vanilla starts with none claimed.
    [[nodiscard]] bool claimed(const std::string& key);

    void claim(const std::string& key);

    /// Writes every map and the counter that changed since the last save.
    void save();

private:
    [[nodiscard]] std::string pathOf(std::string_view name) const;

    std::string directory_;
    std::mutex mutex_;
    /// A miss is kept too, so an unknown id is not a disk read every tick.
    std::map<std::int32_t, std::optional<MapData>> maps_;
    std::optional<std::int32_t> lastId_;
    std::optional<std::set<std::string>> claims_;
    bool claimsDirty_ = false;
    bool indexDirty_ = false;
};

} // namespace basalt::server::runtime::world::maps
