#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/assets/AssetCache.hpp>
#include <basalt/server/assets/VersionMetadata.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace basalt::server::assets;
namespace fileIO = basalt::wire::io::fileIO;

namespace {
[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory{BASALT_TEST_SCRATCH_DIR};
    std::error_code ignored;
    std::filesystem::create_directories(directory, ignored);
    return (directory / name).string();
}
} // namespace

TEST_CASE("the asset cache verifies what it downloaded and trusts its sidecar afterwards") {
    const std::string cacheRoot = scratch("assets-download");
    std::error_code ignored;
    std::filesystem::remove_all(cacheRoot, ignored);

    const std::vector<std::uint8_t> payload{'j', 'a', 'r', ' ', 'b', 'y', 't', 'e', 's'};
    const std::string sha1 = AssetCache::sha1Hex(payload);
    CHECK(sha1.size() == 40);

    VersionMetadata metadata{.id = "26.2",
                             .client = {.url = "https://piston-data/client.jar",
                                        .sha1 = sha1,
                                        .size = static_cast<std::int64_t>(payload.size())},
                             .server = {.url = "https://piston-data/server.jar",
                                        .sha1 = "not-the-hash",
                                        .size = static_cast<std::int64_t>(payload.size())}};

    std::int32_t downloads = 0;
    std::vector<std::size_t> reported;
    AssetCache cache{
        [&](std::string_view, const AssetCache::Progress& onProgress) -> std::optional<std::vector<std::uint8_t>> {
            ++downloads;
            onProgress(payload.size() / 2);
            onProgress(payload.size());
            reported.push_back(payload.size());
            return payload;
        },
        cacheRoot};

    const std::expected<std::string, AssetError> first = cache.ensure(metadata, AssetCache::Artifact::Client);
    REQUIRE(first.has_value());
    CHECK(downloads == 1);
    CHECK(reported.size() == 1);
    CHECK(fileIO::readBytes(*first).value() == payload);

    const std::expected<std::string, AssetError> second = cache.ensure(metadata, AssetCache::Artifact::Client);
    REQUIRE(second.has_value());
    CHECK(downloads == 1);

    const std::expected<std::string, AssetError> wrong = cache.ensure(metadata, AssetCache::Artifact::Server);
    REQUIRE_FALSE(wrong.has_value());
    CHECK(wrong.error().message.find("SHA-1 mismatch") != std::string::npos);
    CHECK_FALSE(fileIO::exists(cacheRoot + "/26.2/server.jar"));
    CHECK(downloads == 2);

    fileIO::writeBytes(*first, std::vector<std::uint8_t>{payload.begin(), payload.begin() + 3});
    CHECK(cache.ensure(metadata, AssetCache::Artifact::Client).has_value());
    CHECK(downloads == 3);

    std::filesystem::remove_all(cacheRoot, ignored);
}
