#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world {
class Chunk;
}

namespace basalt::server::runtime::world {

/// parity: `PoiManager`.
class PortalIndex {
public:
    [[nodiscard]] bool dirty() const;

    [[nodiscard]] std::size_t size() const;

    void scan(const server::world::Chunk& chunk);

    [[nodiscard]] std::optional<std::array<std::int32_t, 3>> nearest(std::int32_t x, std::int32_t y, std::int32_t z,
                                                                     std::int32_t radius) const;

    void load(std::string_view worldRoot);

    void save(std::string_view worldRoot);

private:
    /// Expects `mutex_` to be held: the only caller is `scan`, which takes it.
    void put(std::int64_t chunk, std::vector<std::int64_t> positions);

    /// Every chunk generation worker scans the chunk it just made, so two of them reach `put` at once.
    mutable std::mutex mutex_;
    std::map<std::int64_t, std::vector<std::int64_t>> byChunk_;
    bool dirty_ = false;
};

} // namespace basalt::server::runtime::world
