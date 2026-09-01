#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/world/region/AnvilRegion.hpp>
#include <basalt/wire/io/Compression.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

using namespace basalt::server::world::region;

namespace {

/// Builds a `.mca` the way vanilla lays one out, so the reader is tested against the format rather
/// than against whatever this file happens to produce.
class AnvilBuilder {
public:
    /// `sectorCount` is what the header claims, which is not always what the chunk needs: a chunk
    /// is padded to whole sectors.
    void add(std::int32_t localX, std::int32_t localZ, std::span<const std::uint8_t> payload,
             std::uint8_t compressionType, std::int32_t timestamp) {
        const std::size_t slot = static_cast<std::size_t>(localX) + (static_cast<std::size_t>(localZ) * 32);
        const std::size_t sectorOffset = 2 + (sectors_.size() / 4096);

        std::vector<std::uint8_t> sector;
        const std::uint32_t declared = static_cast<std::uint32_t>(payload.size() + 1);
        sector.push_back(static_cast<std::uint8_t>(declared >> 24));
        sector.push_back(static_cast<std::uint8_t>(declared >> 16));
        sector.push_back(static_cast<std::uint8_t>(declared >> 8));
        sector.push_back(static_cast<std::uint8_t>(declared));
        sector.push_back(compressionType);
        sector.insert(sector.end(), payload.begin(), payload.end());
        sector.resize(((sector.size() + 4095) / 4096) * 4096, 0);
        const std::size_t sectorCount = sector.size() / 4096;
        sectors_.insert(sectors_.end(), sector.begin(), sector.end());

        header_[slot * 4] = static_cast<std::uint8_t>(sectorOffset >> 16);
        header_[(slot * 4) + 1] = static_cast<std::uint8_t>(sectorOffset >> 8);
        header_[(slot * 4) + 2] = static_cast<std::uint8_t>(sectorOffset);
        header_[(slot * 4) + 3] = static_cast<std::uint8_t>(sectorCount);
        timestamps_[slot * 4] = static_cast<std::uint8_t>(timestamp >> 24);
        timestamps_[(slot * 4) + 1] = static_cast<std::uint8_t>(timestamp >> 16);
        timestamps_[(slot * 4) + 2] = static_cast<std::uint8_t>(timestamp >> 8);
        timestamps_[(slot * 4) + 3] = static_cast<std::uint8_t>(timestamp);
    }

    [[nodiscard]] std::vector<std::uint8_t> build() const {
        std::vector<std::uint8_t> out;
        out.insert(out.end(), header_.begin(), header_.end());
        out.insert(out.end(), timestamps_.begin(), timestamps_.end());
        out.insert(out.end(), sectors_.begin(), sectors_.end());
        return out;
    }

    std::vector<std::uint8_t> header_ = std::vector<std::uint8_t>(4096, 0);
    std::vector<std::uint8_t> timestamps_ = std::vector<std::uint8_t>(4096, 0);
    std::vector<std::uint8_t> sectors_;
};

[[nodiscard]] std::vector<std::uint8_t> blob(std::size_t size, std::uint8_t seed) {
    std::vector<std::uint8_t> bytes(size);
    for (std::size_t at = 0; at < size; ++at) {
        bytes[at] = static_cast<std::uint8_t>((at * 17) + seed);
    }
    return bytes;
}

/// The error, or nothing when the read in fact succeeded: asking a successful `expected` for its
/// error is undefined, and a check written that way passes for the wrong reason.
[[nodiscard]] std::optional<AnvilError> errorOf(std::span<const std::uint8_t> bytes) {
    const std::expected<LinearRegion, AnvilError> read = anvilRegion::read(bytes);
    return read.has_value() ? std::nullopt : std::optional{read.error()};
}

} // namespace

TEST_CASE("a vanilla region becomes a linear one, chunk for chunk") {
    const std::vector<std::uint8_t> first = blob(3000, 1);
    const std::vector<std::uint8_t> second = blob(9000, 2);
    AnvilBuilder builder;
    builder.add(0, 0, basalt::wire::io::zlib::compress(first).value(), 2, 111);
    // x fastest, so this one is slot 33 and must not come back at (1, 1) mirrored.
    builder.add(1, 1, basalt::wire::io::zlib::compress(second).value(), 2, 222);
    // Stored uncompressed, which the format allows and nothing writes.
    builder.add(31, 0, blob(64, 3), 3, 333);

    const std::expected<LinearRegion, AnvilError> region = anvilRegion::read(builder.build());
    REQUIRE(region.has_value());
    CHECK(std::ranges::equal(region->chunkAt(0, 0), first));
    CHECK(std::ranges::equal(region->chunkAt(1, 1), second));
    CHECK(std::ranges::equal(region->chunkAt(31, 0), blob(64, 3)));
    CHECK(region->timestampAt(0, 0) == 111);
    CHECK(region->timestampAt(1, 1) == 222);
    CHECK(region->timestampAt(31, 0) == 333);
    // The NBT is carried over untouched, so what it costs is what it cost.
    CHECK(region->residentBytes() == 3000 + 9000 + 64);

    // A slot the header leaves at zero was never generated, and stays that way.
    CHECK_FALSE(region->hasChunkAt(1, 0));
    CHECK_FALSE(region->hasChunkAt(0, 1));
}

TEST_CASE("an empty region file imports as an empty region") {
    const AnvilBuilder builder;
    const std::expected<LinearRegion, AnvilError> region = anvilRegion::read(builder.build());
    REQUIRE(region.has_value());
    CHECK(region->residentBytes() == 0);
}

TEST_CASE("a header entry that points nowhere is an empty slot") {
    AnvilBuilder builder;
    builder.add(4, 4, basalt::wire::io::zlib::compress(blob(100, 8)).value(), 2, 44);
    std::vector<std::uint8_t> bytes = builder.build();
    // An offset with no sectors behind it: half-written by a crash, and vanilla treats it as
    // ungenerated rather than reading whatever is at that offset.
    const std::size_t slot = 4 + (4 * 32);
    bytes[(slot * 4) + 3] = 0;

    const std::expected<LinearRegion, AnvilError> region = anvilRegion::read(bytes);
    REQUIRE(region.has_value());
    CHECK_FALSE(region->hasChunkAt(4, 4));
}

TEST_CASE("a chunk out past the two hundred and fifty-sixth sector is still found") {
    // Three bytes of offset, and a region past a megabyte uses more than the last of them. Every
    // real region of a played-in world is bigger than this.
    const std::vector<std::uint8_t> payload = basalt::wire::io::zlib::compress(blob(200, 9)).value();
    std::vector<std::uint8_t> bytes(300 * 4096, 0);
    const std::size_t sectorOffset = 260;
    bytes[0] = static_cast<std::uint8_t>(sectorOffset >> 16);
    bytes[1] = static_cast<std::uint8_t>(sectorOffset >> 8);
    bytes[2] = static_cast<std::uint8_t>(sectorOffset);
    bytes[3] = 1;
    bytes[4096 + 3] = 55;
    const std::size_t at = sectorOffset * 4096;
    const std::uint32_t declared = static_cast<std::uint32_t>(payload.size() + 1);
    bytes[at] = static_cast<std::uint8_t>(declared >> 24);
    bytes[at + 1] = static_cast<std::uint8_t>(declared >> 16);
    bytes[at + 2] = static_cast<std::uint8_t>(declared >> 8);
    bytes[at + 3] = static_cast<std::uint8_t>(declared);
    bytes[at + 4] = 2;
    std::copy(payload.begin(), payload.end(), bytes.begin() + static_cast<std::ptrdiff_t>(at) + 5);

    const std::expected<LinearRegion, AnvilError> region = anvilRegion::read(bytes);
    REQUIRE(region.has_value());
    CHECK(std::ranges::equal(region->chunkAt(0, 0), blob(200, 9)));
    CHECK(region->timestampAt(0, 0) == 55);
}

TEST_CASE("what an import refuses instead of guessing at") {
    CHECK(errorOf(std::vector<std::uint8_t>(4096, 0)) == AnvilError::TooShort);
    // The timestamps are the second sector, and a file with only the first is still too short.
    CHECK(errorOf(std::vector<std::uint8_t>(6000, 0)) == AnvilError::TooShort);

    // Gzip, which vanilla stopped writing long ago and this does not read.
    AnvilBuilder gzipped;
    gzipped.add(0, 0, blob(100, 4), 1, 1);
    CHECK(errorOf(gzipped.build()) == AnvilError::UnsupportedCompression);

    // A header pointing past the end of the file, which is what a truncated download looks like.
    AnvilBuilder truncated;
    truncated.add(0, 0, basalt::wire::io::zlib::compress(blob(100, 5)).value(), 2, 1);
    std::vector<std::uint8_t> shortened = truncated.build();
    shortened.resize(8192 + 16);
    CHECK(errorOf(shortened) == AnvilError::OutOfBounds);

    // Cut so short that even the chunk's own length header is not there to read.
    std::vector<std::uint8_t> headless = truncated.build();
    headless.resize(8192 + 2);
    CHECK(errorOf(headless) == AnvilError::OutOfBounds);

    // A length that reaches past the file even though the sector it starts in is there.
    AnvilBuilder overlong;
    overlong.add(0, 0, basalt::wire::io::zlib::compress(blob(100, 6)).value(), 2, 1);
    std::vector<std::uint8_t> lying = overlong.build();
    lying[8192] = 0x00;
    lying[8193] = 0x10;
    CHECK(errorOf(lying) == AnvilError::OutOfBounds);

    // Zlib that is not zlib: the container was right and the chunk inside it is rubbish.
    AnvilBuilder rubbish;
    rubbish.add(0, 0, blob(100, 7), 2, 1);
    CHECK(errorOf(rubbish.build()) == AnvilError::CorruptChunk);
}

TEST_CASE("every error has something to say") {
    for (const AnvilError error : {AnvilError::TooShort, AnvilError::OutOfBounds, AnvilError::UnsupportedCompression,
                                   AnvilError::CorruptChunk}) {
        CHECK_FALSE(describe(error).empty());
    }
}
