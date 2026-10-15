#include <basalt/server/runtime/world/PortalIndex.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/portal/PortalShape.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <charconv>
#include <cstdint>
#include <limits>
#include <mutex>

namespace basalt::server::runtime::world {
namespace {

constexpr std::int32_t kSectionHeight = 16;
constexpr std::int32_t kSectionWidth = 16;

[[nodiscard]] std::string pathFor(std::string_view worldRoot) { return std::string{worldRoot} + "/portals.nbt"; }

[[nodiscard]] std::int64_t key(std::int32_t chunkX, std::int32_t chunkZ) {
    return (static_cast<std::int64_t>(chunkX) << 32) | (static_cast<std::int64_t>(chunkZ) & 0xFFFFFFFFLL);
}

[[nodiscard]] std::int64_t pack(std::int32_t x, std::int32_t y, std::int32_t z) {
    return ((static_cast<std::int64_t>(x) & 0x3FFFFFF) << 38) | ((static_cast<std::int64_t>(z) & 0x3FFFFFF) << 12) |
           (static_cast<std::int64_t>(y) & 0xFFF);
}

[[nodiscard]] std::int32_t unpackX(std::int64_t packed) { return static_cast<std::int32_t>(packed >> 38); }

[[nodiscard]] std::int32_t unpackZ(std::int64_t packed) { return static_cast<std::int32_t>(packed << 26 >> 38); }

[[nodiscard]] std::int32_t unpackY(std::int64_t packed) { return static_cast<std::int32_t>(packed << 52 >> 52); }

/// The chunk keys are written as decimal names, which is what has to be read back.
[[nodiscard]] std::optional<std::int64_t> numberOf(std::string_view text) {
    std::int64_t value = 0;
    const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

} // namespace

bool PortalIndex::dirty() const {
    const std::scoped_lock lock{mutex_};
    return dirty_;
}

std::size_t PortalIndex::size() const {
    const std::scoped_lock lock{mutex_};
    std::size_t total = 0;
    for (const auto& [chunk, positions] : byChunk_) {
        total += positions.size();
    }
    return total;
}

void PortalIndex::scan(const server::world::Chunk& chunk) {
    const std::scoped_lock lock{mutex_};
    std::vector<std::int64_t> found;
    for (std::size_t index = 0; index < chunk.sectionCount(); ++index) {
        const server::world::ChunkSection& section = chunk.section(index);
        if (!section.mayContain(
                [](std::int32_t state) { return server::world::portal::PortalShape::isPortal(state); })) {
            continue;
        }
        const std::int32_t sectionMinY =
            chunk.verticalBounds().minY() + (static_cast<std::int32_t>(index) * kSectionHeight);
        for (std::int32_t localY = 0; localY < kSectionHeight; ++localY) {
            for (std::int32_t localZ = 0; localZ < kSectionWidth; ++localZ) {
                for (std::int32_t localX = 0; localX < kSectionWidth; ++localX) {
                    if (!server::world::portal::PortalShape::isPortal(section.blockStateId(localX, localY, localZ))) {
                        continue;
                    }
                    found.push_back(pack((chunk.chunkX() * kSectionWidth) + localX, sectionMinY + localY,
                                         (chunk.chunkZ() * kSectionWidth) + localZ));
                }
            }
        }
    }
    put(key(chunk.chunkX(), chunk.chunkZ()), std::move(found));
}

std::optional<std::array<std::int32_t, 3>> PortalIndex::nearest(std::int32_t x, std::int32_t y, std::int32_t z,
                                                                std::int32_t radius) const {
    const std::scoped_lock lock{mutex_};
    std::optional<std::array<std::int32_t, 3>> best;
    std::int64_t bestDistance = std::numeric_limits<std::int64_t>::max();
    for (std::int32_t chunkX = (x - radius) >> 4; chunkX <= ((x + radius) >> 4); ++chunkX) {
        for (std::int32_t chunkZ = (z - radius) >> 4; chunkZ <= ((z + radius) >> 4); ++chunkZ) {
            const auto found = byChunk_.find(key(chunkX, chunkZ));
            if (found == byChunk_.end()) {
                continue;
            }
            for (const std::int64_t packed : found->second) {
                const std::int32_t blockX = unpackX(packed);
                const std::int32_t blockY = unpackY(packed);
                const std::int32_t blockZ = unpackZ(packed);
                if (blockX < x - radius || blockX > x + radius || blockZ < z - radius || blockZ > z + radius) {
                    continue;
                }
                const std::int64_t dx = blockX - x;
                const std::int64_t dy = blockY - y;
                const std::int64_t dz = blockZ - z;
                const std::int64_t distance = (dx * dx) + (dy * dy) + (dz * dz);
                if (distance >= bestDistance) {
                    continue;
                }
                bestDistance = distance;
                best = std::array<std::int32_t, 3>{blockX, blockY, blockZ};
            }
        }
    }
    return best;
}

void PortalIndex::load(std::string_view worldRoot) {
    const std::scoped_lock lock{mutex_};
    const std::optional<std::vector<std::uint8_t>> bytes = wire::io::fileIO::readBytes(pathFor(worldRoot));
    if (!bytes.has_value()) {
        return;
    }
    const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::read(*bytes);
    if (!read.has_value()) {
        return;
    }
    const wire::nbt::NbtCompound* root = read->tag.get<wire::nbt::NbtCompound>();
    if (root == nullptr) {
        return;
    }
    for (const std::string& name : root->names()) {
        const std::optional<std::int64_t> chunk = numberOf(name);
        if (!chunk.has_value()) {
            continue;
        }
        const std::span<const std::int64_t> positions = root->getLongArray(name);
        if (positions.empty()) {
            continue;
        }
        byChunk_[*chunk] = std::vector<std::int64_t>{positions.begin(), positions.end()};
    }
    dirty_ = false;
}

void PortalIndex::save(std::string_view worldRoot) {
    const std::scoped_lock lock{mutex_};
    if (!dirty_) {
        return;
    }
    wire::nbt::NbtCompound root;
    for (const auto& [chunk, positions] : byChunk_) {
        if (positions.empty()) {
            continue;
        }
        root.putLongArray(std::to_string(chunk), positions);
    }
    static_cast<void>(wire::io::fileIO::writeBytes(
        pathFor(worldRoot), wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(std::move(root)))));
    dirty_ = false;
}

void PortalIndex::put(std::int64_t chunk, std::vector<std::int64_t> positions) {
    const auto previous = byChunk_.find(chunk);
    if (previous != byChunk_.end() && previous->second == positions) {
        return;
    }
    if (positions.empty()) {
        byChunk_.erase(chunk);
    } else {
        byChunk_[chunk] = std::move(positions);
    }
    dirty_ = true;
}

} // namespace basalt::server::runtime::world
