#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace basalt::server::world::region {

/// Why a Linear region file could not be read: bad signature, size mismatch, wrong version.
enum class LinearError : std::uint8_t {
    TooShort,
    BadSignature,
    UnsupportedVersion,
    BadFooter,
    LengthMismatch,
    TruncatedTable,
    TruncatedChunks,
    ChunkCountMismatch,
};

[[nodiscard]] std::string_view describe(LinearError error);

/// One 32x32-chunk Linear region (`.linear`), laid out as `linear.py` has it: a 32-byte big-endian
/// header (signature, version, newest timestamp, compression level, chunk count, compressed length,
/// 8 reserved), one zstd stream over the whole region rather than Anvil's per-chunk zlib, and the
/// signature repeated as a footer. The stream holds 1024 `(size, timestamp)` pairs, row-major with
/// `size == 0` for an empty slot, then the chunk blobs back to back in the same order.
///
/// Only the container: the chunk bytes are opaque here. Coordinates are local (0..31), and mapping
/// them to absolute chunk coordinates is the caller's job.
class LinearRegion {
public:
    static constexpr std::int32_t kDimension = 32;
    static constexpr std::size_t kSlotCount = static_cast<std::size_t>(kDimension) * kDimension;
    static constexpr int kDefaultCompressionLevel = 1;

    LinearRegion() = default;

    [[nodiscard]] static std::expected<LinearRegion, LinearError> read(std::span<const std::uint8_t> bytes);

    /// What this region costs in memory. A cache bounded by region count is bounded by nothing.
    [[nodiscard]] std::int64_t residentBytes() const noexcept { return residentBytes_; }

    [[nodiscard]] std::span<const std::uint8_t> chunkAt(std::int32_t localX, std::int32_t localZ) const;

    [[nodiscard]] bool hasChunkAt(std::int32_t localX, std::int32_t localZ) const;

    [[nodiscard]] std::int32_t timestampAt(std::int32_t localX, std::int32_t localZ) const;

    void setChunk(std::int32_t localX, std::int32_t localZ, std::vector<std::uint8_t> data, std::int32_t timestamp);

    void clearChunk(std::int32_t localX, std::int32_t localZ);

    /// Writes this region straight to `path`, compressing as it goes: peak memory is one output
    /// block plus the 8 KB table whatever the region holds. Building the whole region uncompressed,
    /// then compressed, then framed cost tens of megabytes each time.
    ///
    /// The compressed length is only known at the end, so the header goes out with a hole and is
    /// patched afterwards.
    bool writeTo(const std::string& path, int compressionLevel = kDefaultCompressionLevel) const;

    /// The whole file as bytes. For tests and callers with no path to write to; the server uses
    /// `writeTo`, which never holds a region-sized buffer at all.
    [[nodiscard]] std::vector<std::uint8_t> write(int compressionLevel = kDefaultCompressionLevel) const;

private:
    [[nodiscard]] static std::size_t index(std::int32_t localX, std::int32_t localZ);

    /// The table every write starts with, plus the newest timestamp and the chunk count it declares.
    struct Framing {
        std::vector<std::uint8_t> table;
        std::int64_t newestTimestamp = 0;
        std::int32_t chunkCount = 0;
    };

    [[nodiscard]] Framing framing() const;

    std::array<std::vector<std::uint8_t>, kSlotCount> chunks_;
    std::array<std::int32_t, kSlotCount> timestamps_{};
    std::array<bool, kSlotCount> present_{};
    std::int64_t residentBytes_ = 0;
};

} // namespace basalt::server::world::region
