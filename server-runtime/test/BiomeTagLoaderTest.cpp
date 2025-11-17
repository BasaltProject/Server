#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/BiomeTagLoader.hpp>

#include <doctest.h>

#include <map>

using basalt::server::runtime::world::BiomeTagLoader;

namespace {

/// A jar that holds exactly the entries it was handed, and counts what was asked of it.
struct Jar {
    std::map<std::string, std::string> entries;
    mutable std::int32_t reads = 0;

    [[nodiscard]] BiomeTagLoader::ReadEntry reader() const {
        return [this](std::string_view name) -> std::optional<std::string> {
            ++reads;
            const auto found = entries.find(std::string{name});
            return found == entries.end() ? std::nullopt : std::optional{found->second};
        };
    }
};

[[nodiscard]] std::string tagFile(const std::string& values) { return R"({"values": [)" + values + "]}"; }

} // namespace

TEST_CASE("a tag answers with the biomes it names") {
    Jar jar{.entries = {{"data/minecraft/tags/worldgen/biome/is_overworld.json",
                         tagFile(R"("minecraft:plains", "minecraft:swamp")")}}};
    BiomeTagLoader loader{jar.reader()};

    CHECK(loader.biomesIn("minecraft:is_overworld") == std::set<std::string>{"minecraft:plains", "minecraft:swamp"});
    // The name is read after its namespace, so the bare form finds the same file.
    CHECK(loader.biomesIn("is_overworld") == std::set<std::string>{"minecraft:plains", "minecraft:swamp"});
}

TEST_CASE("a tag that names another tag takes everything in it") {
    Jar jar{.entries = {{"data/minecraft/tags/worldgen/biome/is_overworld.json",
                         tagFile(R"("minecraft:plains", "#minecraft:is_forest")")},
                        {"data/minecraft/tags/worldgen/biome/is_forest.json",
                         tagFile(R"("minecraft:forest", "minecraft:birch_forest")")}}};
    BiomeTagLoader loader{jar.reader()};

    CHECK(loader.biomesIn("minecraft:is_overworld") ==
          std::set<std::string>{"minecraft:plains", "minecraft:forest", "minecraft:birch_forest"});
}

TEST_CASE("a tag that comes back round to itself contributes nothing the second time") {
    Jar jar{.entries = {
                {"data/minecraft/tags/worldgen/biome/one.json", tagFile(R"("minecraft:plains", "#minecraft:two")")},
                {"data/minecraft/tags/worldgen/biome/two.json", tagFile(R"("minecraft:swamp", "#minecraft:one")")}}};
    BiomeTagLoader loader{jar.reader()};

    CHECK(loader.biomesIn("minecraft:one") == std::set<std::string>{"minecraft:plains", "minecraft:swamp"});
}

TEST_CASE("a tag the jar does not hold is empty, and so is a malformed one") {
    Jar jar{.entries = {{"data/minecraft/tags/worldgen/biome/broken.json", "{not json at all"},
                        {"data/minecraft/tags/worldgen/biome/wrong.json", R"({"entries": []})"}}};
    BiomeTagLoader loader{jar.reader()};

    CHECK(loader.biomesIn("minecraft:missing").empty());
    CHECK(loader.biomesIn("minecraft:broken").empty());
    CHECK(loader.biomesIn("minecraft:wrong").empty());
}

TEST_CASE("a tag is read from the jar once and then remembered") {
    Jar jar{.entries = {{"data/minecraft/tags/worldgen/biome/is_overworld.json", tagFile(R"("minecraft:plains")")}}};
    BiomeTagLoader loader{jar.reader()};

    CHECK(loader.biomesIn("minecraft:is_overworld") == std::set<std::string>{"minecraft:plains"});
    CHECK(jar.reads == 1);
    CHECK(loader.biomesIn("minecraft:is_overworld") == std::set<std::string>{"minecraft:plains"});
    CHECK(jar.reads == 1);
    // An empty answer is remembered too, so a missing tag is not looked for twice.
    CHECK(loader.biomesIn("minecraft:missing").empty());
    CHECK(jar.reads == 2);
    CHECK(loader.biomesIn("minecraft:missing").empty());
    CHECK(jar.reads == 2);
}

TEST_CASE("a loader with no jar behind it answers nothing rather than falling over") {
    BiomeTagLoader loader{BiomeTagLoader::ReadEntry{}};

    CHECK(loader.biomesIn("minecraft:is_overworld").empty());
}
