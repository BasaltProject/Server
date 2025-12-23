#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/VanillaSaveImport.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/io/Compression.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::world::ImportReport;
using basalt::server::runtime::world::importVanillaSave;
using basalt::server::storage::PlayerStore;
namespace nbt = basalt::wire::nbt;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::filesystem::create_directories(directory, ignored);
    return directory.string();
}

void writeText(const std::string& path, const std::string& text) {
    std::ofstream file{path};
    file << text;
}

/// A vanilla `playerdata/<uuid>.dat`: gzipped binary NBT, which is what the real save holds.
void writePlayerData(const std::string& path, float health) {
    nbt::NbtCompound root;
    root.set("Health", nbt::NbtTag::ofFloat(health));
    root.set("playerGameType", nbt::NbtTag::ofInt(1));
    nbt::NbtList position{nbt::NbtType::Double};
    for (const double one : {8.5, 64.0, -12.5}) {
        position.push(nbt::NbtTag::ofDouble(one));
    }
    root.set("Pos", nbt::NbtTag::ofList(std::move(position)));
    const std::vector<std::uint8_t> plain = nbt::nbtIO::write(nbt::NbtTag::ofCompound(std::move(root)));
    const auto gzipped = basalt::wire::io::gzip::compress(plain);
    REQUIRE(gzipped.has_value());
    std::ofstream file{path, std::ios::binary};
    file.write(reinterpret_cast<const char*>(gzipped->data()), static_cast<std::streamsize>(gzipped->size()));
}

} // namespace

TEST_CASE("an import reads the user cache, skips whoever has no data file, and writes the rest") {
    const std::string save = scratch("import-save");
    const std::string database = scratch("import-db");
    std::error_code ignored;
    std::filesystem::create_directories(save + "/playerdata", ignored);

    const std::string steve = "b1e0c0de-0000-4000-8000-00000000abcd";
    const std::string alex = "b1e0c0de-0000-4000-8000-00000000abce";
    writeText(save + "/usercache.json",
              R"([{"name":"Steve","uuid":")" + steve + R"("},{"name":"Alex","uuid":")" + alex + R"("}])");
    writePlayerData(save + "/playerdata/" + steve + ".dat", 17.5F);

    auto opened = PlayerStore::open(database + "/players.db");
    REQUIRE(opened.has_value());

    const std::expected<ImportReport, std::string> report =
        importVanillaSave(save, (*opened)->players(), "world", 1700000000LL);
    REQUIRE(report.has_value());
    CHECK(report->imported == 1);
    // Alex is in the cache but never joined, which is normal rather than a failure.
    CHECK(report->skipped == 1);
    CHECK(report->failed == 0);

    const std::optional<basalt::server::storage::StoredPlayer> loaded = (*opened)->players().load(steve);
    REQUIRE(loaded.has_value());
    CHECK(loaded->name == "Steve");
    CHECK(loaded->health == 17.5F);
    CHECK(loaded->world == "world");

    // A second run overwrites the same row rather than adding another.
    const std::expected<ImportReport, std::string> again =
        importVanillaSave(save, (*opened)->players(), "world", 1700000001LL);
    REQUIRE(again.has_value());
    CHECK(again->imported == 1);
}

TEST_CASE("an import says why it could not start instead of reporting nothing imported") {
    const std::string save = scratch("import-empty");
    const std::string database = scratch("import-empty-db");
    auto opened = PlayerStore::open(database + "/players.db");
    REQUIRE(opened.has_value());

    const std::expected<ImportReport, std::string> missing = importVanillaSave(save, (*opened)->players(), "world", 0);
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().find("usercache.json not found") != std::string::npos);

    writeText(save + "/usercache.json", R"({"not":"an array"})");
    const std::expected<ImportReport, std::string> wrongShape =
        importVanillaSave(save, (*opened)->players(), "world", 0);
    REQUIRE_FALSE(wrongShape.has_value());
    CHECK(wrongShape.error().find("not the JSON array") != std::string::npos);
}

TEST_CASE("one unreadable data file is counted, not fatal to the rest of the import") {
    const std::string save = scratch("import-corrupt");
    const std::string database = scratch("import-corrupt-db");
    std::error_code ignored;
    std::filesystem::create_directories(save + "/playerdata", ignored);

    const std::string steve = "b1e0c0de-0000-4000-8000-00000000abcd";
    const std::string alex = "b1e0c0de-0000-4000-8000-00000000abce";
    writeText(save + "/usercache.json",
              R"([{"name":"Steve","uuid":")" + steve + R"("},{"name":"Alex","uuid":")" + alex + R"("}])");
    writePlayerData(save + "/playerdata/" + steve + ".dat", 20.0F);
    writeText(save + "/playerdata/" + alex + ".dat", "this is not gzipped NBT");

    auto opened = PlayerStore::open(database + "/players.db");
    REQUIRE(opened.has_value());
    const std::expected<ImportReport, std::string> report =
        importVanillaSave(save, (*opened)->players(), "world", 1700000000LL);
    REQUIRE(report.has_value());
    CHECK(report->imported == 1);
    CHECK(report->failed == 1);
}
