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

/// Where the portals of one world are, chunk by chunk, whether or not that chunk is in memory.
///
/// parity: `PoiManager`'s `nether_portal` index, which is what lets a portal on the other side be
/// found again instead of a second one being built beside it. This keeps the same promise with far
/// less machinery: a chunk's entry is rewritten every time the chunk is scanned, so what a loaded
/// chunk says is always current, and the rest survives on disk in `portals.nbt`.
class PortalIndex {
public:
    /// Whether anything has changed since the last load or save.
    [[nodiscard]] bool dirty() const;

    /// How many portal blocks the index knows of, over every chunk.
    [[nodiscard]] std::size_t size() const;

    /// Replaces what is known about a chunk with what its blocks actually say.
    void scan(const server::world::Chunk& chunk);

    /// The nearest portal block to a point, or nothing when the index knows of none in range.
    [[nodiscard]] std::optional<std::array<std::int32_t, 3>> nearest(std::int32_t x, std::int32_t y, std::int32_t z,
                                                                     std::int32_t radius) const;

    /// Reads `portals.nbt`, which is what a chunk no longer in memory is remembered by.
    void load(std::string_view worldRoot);

    void save(std::string_view worldRoot);

private:
    /// Expects `mutex_` to be held: the only caller is `scan`, which takes it.
    void put(std::int64_t chunk, std::vector<std::int64_t> positions);

    /// Every chunk generation worker scans the chunk it just made, so two of them reach `put` at
    /// once. Without this the map itself is mutated from several threads, not just the flag.
    mutable std::mutex mutex_;
    std::map<std::int64_t, std::vector<std::int64_t>> byChunk_;
    bool dirty_ = false;
};

} // namespace basalt::server::runtime::world
