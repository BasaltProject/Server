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

/// Shared across world threads, so every call holds `lock()`.
class MapStorage {
public:
    static constexpr std::string_view kDefaultDirectory = "maps";

    explicit MapStorage(std::string directory = std::string{kDefaultDirectory}) : directory_(std::move(directory)) {}

    [[nodiscard]] std::unique_lock<std::mutex> lock() { return std::unique_lock{mutex_}; }

    [[nodiscard]] MapData* get(std::int32_t id);

    void set(std::int32_t id, MapData data);

    template <typename Visit>
    void forEachLoaded(Visit&& visit) {
        for (auto& [id, data] : maps_) {
            if (data.has_value()) {
                visit(id, *data);
            }
        }
    }

    [[nodiscard]] std::int32_t nextId();

    /// parity: `StructureStart.references`.
    [[nodiscard]] bool claimed(const std::string& key);

    void claim(const std::string& key);

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
