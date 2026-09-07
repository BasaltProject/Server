#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/assets/AssetCache.hpp>
#include <basalt/server/assets/EulaFile.hpp>
#include <basalt/server/assets/ServerJarAssets.hpp>
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

[[nodiscard]] std::string describe(const VersionMetadata& metadata) {
    return metadata.id + " " + metadata.client.url + "/" + metadata.client.sha1 + "/" +
           std::to_string(metadata.client.size) + " " + metadata.server.url + "/" + metadata.server.sha1 + "/" +
           std::to_string(metadata.server.size);
}

} // namespace

TEST_CASE("the eula file is read the way the frozen kotlin reads it") {
    CHECK(eulaFile::isAccepted("eula = true"));
    CHECK(eulaFile::isAccepted("eula=true"));
    CHECK(eulaFile::isAccepted("EULA = TRUE"));
    CHECK(eulaFile::isAccepted("  eula   =   true  "));
    CHECK(eulaFile::isAccepted("# a comment\n# eula = true\neula = true\n"));
    CHECK_FALSE(eulaFile::isAccepted("eula = false"));
    // Every line here says true inside a comment, which is not agreement.
    CHECK_FALSE(eulaFile::isAccepted("#eula = true\n# eula=true\n   #eula = true"));
    CHECK_FALSE(eulaFile::isAccepted(""));
    CHECK_FALSE(eulaFile::isAccepted("eula-accepted = true\nseula = true\neula = truex"));
    CHECK(eulaFile::isAccepted("#By changing the setting below to TRUE you are indicating your agreement to our EULA\n"
                               "#Mon Jan 01 00:00:00 UTC 2024\neula=true\n"));

    CHECK(eulaFile::kPath == "eula.toml");
    CHECK(eulaFile::kLegacyPath == "eula.txt");
    CHECK(eulaFile::kRejectedExitCode == 78);
}

TEST_CASE("the eula file is written and migrated the way the frozen kotlin writes it") {
    // `check` reads and writes in the working directory, which is the only reason this moves into
    // the scratch one. What it decides with is `isAccepted`, which the case above pins against the
    // frozen Kotlin; the Kotlin cannot run this half itself without writing into its own repo.
    const std::filesystem::path directory{scratch("assets-eula")};
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::filesystem::create_directories(directory, ignored);
    const std::filesystem::path previous = std::filesystem::current_path();
    std::filesystem::current_path(directory);

    const basalt::wire::common::Logger logger{"EulaTest"};

    // The flag is a deliberate operator act, and does not touch the file.
    CHECK(eulaFile::check(true, logger) == eulaFile::Outcome::Accepted);
    CHECK_FALSE(fileIO::exists(std::string{eulaFile::kPath}));

    // No file at all: one is written saying no, and the server is told to stop.
    CHECK(eulaFile::check(false, logger) == eulaFile::Outcome::JustCreated);
    const std::optional<std::string> written = fileIO::readText(std::string{eulaFile::kPath});
    REQUIRE(written.has_value());
    CHECK(written->find("eula = false") != std::string::npos);
    CHECK(written->find("https://www.minecraft.net/en-us/eula") != std::string::npos);
    CHECK_FALSE(eulaFile::isAccepted(*written));

    // The file is there and says no: rejected, and not created a second time.
    CHECK(eulaFile::check(false, logger) == eulaFile::Outcome::Rejected);

    fileIO::writeText(std::string{eulaFile::kPath}, "eula = true");
    CHECK(eulaFile::check(false, logger) == eulaFile::Outcome::Accepted);

    // A hosting panel's own eula.txt is migrated once, and the new file wins from then on.
    fileIO::remove(std::string{eulaFile::kPath});
    fileIO::writeText(std::string{eulaFile::kLegacyPath}, "#By changing the setting below to TRUE\neula=true\n");
    CHECK(eulaFile::check(false, logger) == eulaFile::Outcome::Accepted);
    CHECK(eulaFile::isAccepted(fileIO::readText(std::string{eulaFile::kPath}).value()));

    // A legacy file that says no is not agreement, and does not stop the new one being written.
    fileIO::remove(std::string{eulaFile::kPath});
    fileIO::writeText(std::string{eulaFile::kLegacyPath}, "eula=false");
    CHECK(eulaFile::check(false, logger) == eulaFile::Outcome::JustCreated);
    CHECK_FALSE(eulaFile::isAccepted(fileIO::readText(std::string{eulaFile::kPath}).value()));

    std::filesystem::current_path(previous);
    std::filesystem::remove_all(directory, ignored);
}

TEST_CASE("version metadata parses both shapes and writes back the cached one") {
    // The shape piston-meta answers with: the size is a number.
    const std::expected<VersionMetadata, AssetError> piston = versionManifest::parse("26.2", R"({"downloads":{
                     "client":{"url":"https://piston-data/client.jar","sha1":"aaaa1111","size":26000000},
                     "server":{"url":"https://piston-data/server.jar","sha1":"bbbb2222","size":52000000}}})");
    REQUIRE(piston.has_value());
    CHECK(describe(*piston) ==
          "26.2 https://piston-data/client.jar/aaaa1111/26000000 https://piston-data/server.jar/bbbb2222/52000000");

    // The shape this project writes when it caches one: the size is a string.
    const std::expected<VersionMetadata, AssetError> cached = versionManifest::parse("26.2", R"({"downloads":{
                     "client":{"url":"https://piston-data/client.jar","sha1":"aaaa1111","size":"26000000"},
                     "server":{"url":"https://piston-data/server.jar","sha1":"bbbb2222","size":"52000000"}}})");
    REQUIRE(cached.has_value());
    CHECK(describe(*cached) == describe(*piston));

    CHECK(piston->toJson() ==
          R"({"downloads":{"client":{"url":"https://piston-data/client.jar","sha1":"aaaa1111","size":"26000000"},)"
          R"("server":{"url":"https://piston-data/server.jar","sha1":"bbbb2222","size":"52000000"}}})");
    // What it writes is what it reads: a cached copy goes back through the same parser.
    const std::expected<VersionMetadata, AssetError> roundTrip = versionManifest::parse("26.2", piston->toJson());
    REQUIRE(roundTrip.has_value());
    CHECK(describe(*roundTrip) == describe(*piston));

    CHECK_FALSE(versionManifest::parse("26.2", R"({"downloads":{"client":{}}})").has_value());
    // Every field present but one of the wrong type, which is not the same as absent.
    CHECK_FALSE(versionManifest::parse("26.2", R"({"downloads":{
                     "client":{"url":123,"sha1":"aaaa1111","size":26000000},
                     "server":{"url":"https://piston-data/server.jar","sha1":"bbbb2222","size":52000000}}})")
                    .has_value());
    CHECK_FALSE(versionManifest::parse("26.2", "not json").has_value());
    CHECK_FALSE(versionManifest::parse("26.2", R"({})").has_value());
}

TEST_CASE("the manifest is walked to the version asked for, and no further") {
    const std::string manifest = R"({"versions":[
        {"id":"26.1","url":"https://piston-meta/26.1.json"},
        {"id":"26.2","url":"https://piston-meta/26.2.json"}]})";
    const std::string metadata = R"({"downloads":{
        "client":{"url":"https://piston-data/client.jar","sha1":"aaaa1111","size":26000000},
        "server":{"url":"https://piston-data/server.jar","sha1":"bbbb2222","size":52000000}}})";

    std::vector<std::string> asked;
    const versionManifest::Fetcher fetch = [&](std::string_view url) -> std::optional<std::string> {
        asked.emplace_back(url);
        if (url == versionManifest::kVersionManifestUrl) {
            return manifest;
        }
        if (url == "https://piston-meta/26.2.json") {
            return metadata;
        }
        return std::nullopt;
    };

    const std::expected<VersionMetadata, AssetError> resolved = versionManifest::resolve("26.2", fetch);
    REQUIRE(resolved.has_value());
    CHECK(describe(*resolved) ==
          "26.2 https://piston-data/client.jar/aaaa1111/26000000 https://piston-data/server.jar/bbbb2222/52000000");
    CHECK(asked.size() == 2);
    CHECK(asked[1] == "https://piston-meta/26.2.json");

    const std::expected<VersionMetadata, AssetError> unknown = versionManifest::resolve("1.7.10", fetch);
    REQUIRE_FALSE(unknown.has_value());
    CHECK(unknown.error().message == "Unknown Minecraft version: 1.7.10");

    // A cached copy is read back without a single request, which is what lets a warm install start
    // with no network at all.
    const std::string cacheRoot = scratch("assets-cache");
    std::error_code ignored;
    std::filesystem::remove_all(cacheRoot, ignored);
    asked.clear();
    CHECK(versionManifest::resolveCached("26.2", fetch, cacheRoot).has_value());
    CHECK(asked.size() == 2);
    asked.clear();
    const std::expected<VersionMetadata, AssetError> warm = versionManifest::resolveCached("26.2", fetch, cacheRoot);
    REQUIRE(warm.has_value());
    CHECK(describe(*warm) == describe(*resolved));
    CHECK(asked.empty());

    // A resolve that failed writes nothing: the next boot has to try again, not read a file that
    // was never resolved.
    const versionManifest::Fetcher broken = [](std::string_view) { return std::optional<std::string>{}; };
    CHECK_FALSE(versionManifest::resolveCached("1.7.10", broken, cacheRoot).has_value());
    CHECK_FALSE(fileIO::exists(cacheRoot + "/1.7.10/metadata.json"));
    std::filesystem::remove_all(cacheRoot, ignored);
}

TEST_CASE("the asset cache verifies what it downloaded and trusts its sidecar afterwards") {
    const std::string cacheRoot = scratch("assets-download");
    std::error_code ignored;
    std::filesystem::remove_all(cacheRoot, ignored);

    const std::vector<std::uint8_t> payload{'j', 'a', 'r', ' ', 'b', 'y', 't', 'e', 's'};
    const std::string sha1 = AssetCache::sha1Hex(payload);
    // Derived from the bytes rather than written from memory: a hash typed by hand is a test that
    // fails against correct code.
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
    // The fetcher is handed somewhere to report to, so a download can say how far along it is.
    CHECK(reported.size() == 1);
    CHECK(fileIO::readBytes(*first).value() == payload);

    // The sidecar matches and so does the size, so the second call reads nothing off the network.
    const std::expected<std::string, AssetError> second = cache.ensure(metadata, AssetCache::Artifact::Client);
    REQUIRE(second.has_value());
    CHECK(downloads == 1);

    // A hash that does not match is refused, and nothing is written.
    const std::expected<std::string, AssetError> wrong = cache.ensure(metadata, AssetCache::Artifact::Server);
    REQUIRE_FALSE(wrong.has_value());
    CHECK(wrong.error().message.find("SHA-1 mismatch") != std::string::npos);
    CHECK_FALSE(fileIO::exists(cacheRoot + "/26.2/server.jar"));
    CHECK(downloads == 2);

    // A truncated file is caught by the size even though the sidecar still says it is fine.
    fileIO::writeBytes(*first, std::vector<std::uint8_t>{payload.begin(), payload.begin() + 3});
    CHECK(cache.ensure(metadata, AssetCache::Artifact::Client).has_value());
    CHECK(downloads == 3);

    std::filesystem::remove_all(cacheRoot, ignored);
}

TEST_CASE("a warm data cache is read from its index and never resolves its source") {
    const std::string dataRoot = scratch("assets-data");
    std::error_code ignored;
    std::filesystem::remove_all(dataRoot, ignored);
    fileIO::createDirectories(dataRoot + "/worldgen/noise");

    const std::string index = "data/minecraft/worldgen/noise/continentalness.json\n"
                              "data/minecraft/worldgen/noise/erosion.json\n"
                              "data/minecraft/worldgen/biome/plains.json\n"
                              "data/minecraft/loot_table/blocks/stone.json\n"
                              "data/basalt/worldgen/noise/custom.json\n"
                              "data/minecraft/tags/block/logs.json\n"
                              "assets/minecraft/lang/en_us.json\n"
                              // None of these is under a namespace directory, so no id comes out of them.
                              "data/pack.mcmeta\n"
                              "data//worldgen/noise/nameless.json\n"
                              "data/minecraft/worldgen/noise/not_json.txt";
    fileIO::writeText(dataRoot + "/.index", index);
    fileIO::writeText(dataRoot + "/.unpacked", "26.2 v2-lang");
    fileIO::writeText(dataRoot + "/worldgen/noise/continentalness.json", R"({"firstOctave":-9})");

    bool resolved = false;
    const std::expected<ServerJarAssets, AssetError> assets =
        ServerJarAssets::open("26.2", dataRoot, [&]() -> std::expected<std::string, AssetError> {
            resolved = true;
            return std::unexpected(AssetError{"a warm cache must not resolve its source"});
        });
    REQUIRE(assets.has_value());
    CHECK_FALSE(resolved);

    const std::vector<std::string> entries = assets->entriesUnder("data/minecraft/worldgen/noise/");
    REQUIRE(entries.size() == 3);
    CHECK(entries[0] == "data/minecraft/worldgen/noise/continentalness.json");
    CHECK(entries[2] == "data/minecraft/worldgen/noise/not_json.txt");

    std::vector<std::string> ids;
    for (const basalt::wire::common::Identifier& id : assets->dataIds("worldgen/noise", ".json")) {
        ids.push_back(id.toString());
    }
    std::sort(ids.begin(), ids.end());
    REQUIRE(ids.size() == 3);
    CHECK(ids[0] == "basalt:custom");
    CHECK(ids[1] == "minecraft:continentalness");
    CHECK(ids[2] == "minecraft:erosion");

    std::vector<std::string> biomes;
    for (const basalt::wire::common::Identifier& id : assets->dataIds("worldgen/biome", ".json")) {
        biomes.push_back(id.toString());
    }
    REQUIRE(biomes.size() == 1);
    CHECK(biomes[0] == "minecraft:plains");

    CHECK(assets->readText("data/minecraft/worldgen/noise/continentalness.json").value() == R"({"firstOctave":-9})");
    // In the index but not on disk: absent, not an empty answer.
    CHECK_FALSE(assets->readEntry("data/minecraft/worldgen/noise/erosion.json").has_value());
    CHECK_FALSE(assets->readText("data/minecraft/worldgen/noise/erosion.json").has_value());

    // A marker from a build that unpacked a narrower set has to redo the unpack, not be trusted.
    fileIO::writeText(dataRoot + "/.unpacked", "26.2 v1");
    resolved = false;
    CHECK_FALSE(ServerJarAssets::open("26.2", dataRoot, [&]() -> std::expected<std::string, AssetError> {
                    resolved = true;
                    return std::unexpected(AssetError{"cold"});
                }).has_value());
    CHECK(resolved);

    std::filesystem::remove_all(dataRoot, ignored);
}
