#pragma once

#include <basalt/server/runtime/world/RegionStore.hpp>
#include <basalt/server/world/blending/Blender.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace basalt::server::runtime::world {

class OldTerrainBlending {
public:
    explicit OldTerrainBlending(RegionStore& regions) : regions_(&regions) {}

    /// parity: `Blender.of`; null when there is nothing to blend with.
    [[nodiscard]] std::shared_ptr<const server::world::blending::Blender> blenderFor(std::int32_t chunkX,
                                                                                     std::int32_t chunkZ);

private:
    struct Entry {
        bool classified = false;
        std::shared_ptr<server::world::blending::BlendingData> data;
    };

    [[nodiscard]] Entry& classify(std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] std::shared_ptr<server::world::blending::BlendingData> calculated(std::int32_t chunkX,
                                                                                    std::int32_t chunkZ);

    RegionStore* regions_;
    std::mutex mutex_;
    std::unordered_map<std::int64_t, Entry> entries_;
    std::unordered_map<std::int64_t, RegionStore::Age> ages_;
};

} // namespace basalt::server::runtime::world
