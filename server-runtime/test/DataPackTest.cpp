#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/datapack/DataPackSelection.hpp>
#include <basalt/server/runtime/datapack/DataPacks.hpp>
#include <basalt/server/runtime/datapack/PackMetadata.hpp>
#include <basalt/server/runtime/datapack/PackRepository.hpp>
#include <basalt/server/runtime/datapack/RegistrySchemas.hpp>
#include <basalt/server/runtime/datapack/ReloadableElements.hpp>
#include <basalt/server/runtime/datapack/ResourceStack.hpp>
#include <basalt/server/runtime/registry/PackRegistries.hpp>
#include <basalt/server/runtime/registry/VanillaConfigurationRegistries.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <doctest.h>

#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

using namespace basalt::server::runtime::datapack;
using basalt::wire::common::Identifier;
namespace fileIO = basalt::wire::io::fileIO;

namespace {

constexpr std::string_view kCurrentMeta = R"({"pack":{"description":"x","min_format":121,"max_format":121}})";

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / "datapack" / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::filesystem::create_directories(directory, ignored);
    return directory.string();
}

void write(const std::string& path, std::string_view text) {
    fileIO::createDirectories(path.substr(0, path.rfind('/')));
    fileIO::writeText(path, text);
}

/// A jar stand-in: a map of entry names to their text.
[[nodiscard]] PackFiles memoryFiles(std::map<std::string, std::string> entries) {
    auto shared = std::make_shared<const std::map<std::string, std::string>>(std::move(entries));
    return PackFiles{
        .list =
            [shared] {
                std::vector<std::string> names;
                for (const auto& [name, unused] : *shared) {
                    names.push_back(name);
                }
                return names;
            },
        .read = [shared](std::string_view name) -> std::optional<std::vector<std::uint8_t>> {
            const auto found = shared->find(std::string{name});
            if (found == shared->end()) {
                return std::nullopt;
            }
            return std::vector<std::uint8_t>{found->second.begin(), found->second.end()};
        },
    };
}

[[nodiscard]] PackFiles fakeJar() {
    return memoryFiles({
        {"data/minecraft/loot_table/a.json", "vanilla-a"},
        {"data/minecraft/loot_table/b.json", "vanilla-b"},
        {"data/minecraft/tags/item/logs.json", R"({"values":["minecraft:oak_log"]})"},
        {"data/minecraft/datapacks/trade_rebalance/pack.mcmeta",
         R"({"features":{"enabled":["minecraft:trade_rebalance"]},"pack":{"description":"t","min_format":121,"max_format":121}})"},
        {"data/minecraft/datapacks/trade_rebalance/data/minecraft/loot_table/a.json", "rebalanced-a"},
    });
}

} // namespace

TEST_CASE("pack.mcmeta formats are checked the way 26.3 checks them") {
    CHECK(parsePackMetadata(kCurrentMeta)->supported.min == PackFormat{121, 0});
    CHECK(compatibilityOf(parsePackMetadata(kCurrentMeta)->supported) == Compatibility::Compatible);
    // A pre-minor pack_format alone is its own single version, too old for 121.
    CHECK(compatibilityOf(parsePackMetadata(R"({"pack":{"description":"","pack_format":48}})")->supported) ==
          Compatibility::TooOld);
    // A missing min/max pair falls back to the description: listed, of unknown compatibility.
    CHECK(compatibilityOf(parsePackMetadata(R"({"pack":{"description":"","min_format":121}})")->supported) ==
          Compatibility::Unknown);
    CHECK(compatibilityOf(
              parsePackMetadata(R"({"pack":{"description":"","min_format":[122,1],"max_format":200}})")->supported) ==
          Compatibility::TooNew);
    // Spanning the pre-minor versions needs supported_formats and pack_format to agree with them.
    CHECK(compatibilityOf(parsePackMetadata(R"({"pack":{"description":"","min_format":71,"max_format":121,
        "supported_formats":[71,81],"pack_format":71}})")
                              ->supported) == Compatibility::Compatible);
    CHECK(compatibilityOf(
              parsePackMetadata(R"({"pack":{"description":"","min_format":71,"max_format":121}})")->supported) ==
          Compatibility::Unknown);
    CHECK_FALSE(parsePackMetadata(R"({"notpack":{}})").has_value());
    CHECK_FALSE(parsePackMetadata("{not json").has_value());
    CHECK_FALSE(
        parsePackMetadata(R"({"pack":{"description":"","pack_format":1},"features":{"enabled":["x:y"]}})").has_value());
}

TEST_CASE("only overlays whose range holds 121 apply, in file order") {
    const auto metadata = parsePackMetadata(R"({"pack":{"description":"","min_format":100,"max_format":130},
        "overlays":{"entries":[{"directory":"old","min_format":100,"max_format":110},
                               {"directory":"now","min_format":121,"max_format":121},
                               {"directory":"later","min_format":[121,0],"max_format":130}]}})");
    REQUIRE(metadata.has_value());
    CHECK(metadata->overlays == std::vector<std::string>{"now", "later"});
    CHECK_FALSE(parsePackMetadata(R"({"pack":{"description":"","pack_format":1},
        "overlays":{"entries":[{"directory":"bad name!","min_format":121,"max_format":121}]}})")
                    .has_value());
}

TEST_CASE("the stack reads the top pack, filters hide lower ones and tags merge") {
    const PackFiles bottom =
        memoryFiles({{"data/minecraft/loot_table/a.json", "bottom-a"},
                     {"data/minecraft/loot_table/b.json", "bottom-b"},
                     {"data/minecraft/tags/item/logs.json", R"({"values":["minecraft:oak_log"]})"}});
    const PackFiles top = memoryFiles(
        {{"pack.mcmeta", R"({"pack":{"description":""},"filter":{"block":[{"path":"loot_table/b"}]}})"},
         {"data/minecraft/loot_table/a.json", "top-a"},
         {"over/data/minecraft/loot_table/a.json", "overlay-a"},
         {"data/minecraft/tags/item/logs.json", R"({"values":["minecraft:birch_log",{"id":"#x:y","required":false}]})"},
         {"data/mine/tags/item/own.json", R"({"replace":true,"values":["minecraft:stone"]})"},
         {"data/BAD/loot_table/c.json", "ignored"}});
    ResourceStack stack{{
        ResourceStack::Layer{.contents = std::make_shared<const PackContents>(
                                 "bottom", bottom, std::vector<std::string>{}, basalt::wire::common::Logger{"Test"}),
                             .filter = {}},
        ResourceStack::Layer{.contents = std::make_shared<const PackContents>(
                                 "top", top, std::vector<std::string>{"over"}, basalt::wire::common::Logger{"Test"}),
                             .filter = *parseFilter(R"({"filter":{"block":[{"path":"loot_table/b"}]}})")},
    }};
    CHECK(stack.readText("data/minecraft/loot_table/a.json") == "overlay-a");
    CHECK_FALSE(stack.readText("data/minecraft/loot_table/b.json").has_value());
    CHECK(stack.entriesUnder("data/minecraft/loot_table/") ==
          std::vector<std::string>{"data/minecraft/loot_table/a.json"});
    CHECK(stack.readText("data/minecraft/tags/item/logs.json") ==
          R"({"values":["minecraft:oak_log","minecraft:birch_log",{"id":"#x:y","required":false}]})");
    CHECK(stack.dataIds("tags/item", ".json") ==
          std::vector<Identifier>{Identifier{"mine", "own"}, Identifier{"minecraft", "logs"}});
    CHECK(stack.packOf("data/minecraft/loot_table/a.json") == "top");
}

TEST_CASE("the repository finds the jar's packs and the world's, and a new world selects as vanilla does") {
    const std::string root = scratch("repository");
    write(root + "/datapacks/mine/pack.mcmeta", kCurrentMeta);
    write(root + "/datapacks/mine/data/minecraft/loot_table/b.json", "mine-b");
    write(root + "/datapacks/notapack/readme.txt", "");
    write(
        root + "/datapacks/flagged/pack.mcmeta",
        R"({"features":{"enabled":["minecraft:redstone_experiments"]},"pack":{"description":"","min_format":121,"max_format":121}})");

    DataPacks packs{DataPacks::Options{.jar = fakeJar(), .worldRoot = root, .initial = {}, .safeMode = false}};
    CHECK(packs.repository().availableIds() ==
          std::vector<std::string>{"file/flagged", "file/mine", "trade_rebalance", "vanilla"});
    // A new world may use any feature, so the flagged pack loads; the feature pack never adds itself.
    CHECK(packs.repository().selectedIds() == std::vector<std::string>{"vanilla", "file/flagged", "file/mine"});
    CHECK(packs.configuration().dataPacks.disabled == std::vector<std::string>{"trade_rebalance"});
    CHECK(packs.resources()->readText("data/minecraft/loot_table/b.json") == "mine-b");
    CHECK(packs.resources()->readText("data/minecraft/loot_table/a.json") == "vanilla-a");

    // The selection is saved, and an existing world only keeps packs its features allow.
    WorldDataConfiguration saved = *WorldDataConfiguration::read(root);
    saved.enabledFeatures = {Identifier::vanilla("vanilla"), Identifier::vanilla("trade_rebalance")};
    saved.dataPacks.enabled = {"vanilla", "file/flagged", "trade_rebalance"};
    saved.dataPacks.disabled = {};
    REQUIRE(saved.writeTo(root));
    DataPacks reopened{DataPacks::Options{.jar = fakeJar(), .worldRoot = root, .initial = {}, .safeMode = false}};
    CHECK(reopened.repository().selectedIds() == std::vector<std::string>{"vanilla", "trade_rebalance", "file/mine"});
    CHECK(reopened.resources()->readText("data/minecraft/loot_table/a.json") == "rebalanced-a");
}

TEST_CASE("installed registries go out in id order, a pack's entries among vanilla's, and number the server's lists") {
    std::vector<basalt::wire::protocol::configuration::RegistryDataPacket> packets =
        basalt::server::runtime::registry::vanillaConfigurationRegistries::packets();
    for (auto& packet : packets) {
        if (packet.registryId == "minecraft:wolf_variant") {
            packet.entries.push_back({.key = "minecraft:blue", .value = std::nullopt});
        }
    }
    basalt::server::runtime::registry::installRegistries(std::move(packets));
    const std::vector<std::string>& wolves = basalt::server::runtime::registry::vanillaRegistryData::wolfVariants();
    REQUIRE(wolves.size() == 10);
    CHECK(wolves.front() == "ashen");
    CHECK(wolves[2] == "blue");
    CHECK(std::ranges::is_sorted(wolves));
}

TEST_CASE("an element missing what its codec requires is refused, and one that has it passes") {
    namespace datapack = basalt::server::runtime::datapack;
    const auto object = [](std::string_view text) {
        return *basalt::wire::common::parseJson(text)->get<basalt::wire::common::JsonObject>();
    };
    CHECK_FALSE(datapack::schemaError("wolf_variant", object(R"({"assets":{"wild":"a:b","tame":"a:c","angry":"a:d"},
        "baby_assets":{"wild":"a:b","tame":"a:c","angry":"a:d"},
        "spawn_conditions":[{"priority":1,"condition":{"type":"minecraft:biome","biomes":"#minecraft:is_savanna"}}]})"))
                    .has_value());
    CHECK(datapack::schemaError("wolf_variant", object(R"({"assets":{"wild":"a:b"}})")) == "assets: No key tame");
    CHECK(datapack::schemaError(
              "frog_variant",
              object(R"({"asset_id":"a:b","spawn_conditions":[{"condition":{"type":"x:y"},"priority":0}]})"))
              .value_or("")
              .starts_with("spawn_conditions: condition: Unknown spawn condition type"));
    CHECK(datapack::schemaError("painting_variant", object(R"({"width":"1","height":1,"asset_id":"a:b"})")) ==
          "width: Not a number");
    // A registry without a schema here is not judged.
    CHECK_FALSE(datapack::schemaError("timeline", object("{}")).has_value());
}

TEST_CASE("a reloadable element fails on what vanilla does not know, not on what this build has yet to model") {
    const PackFiles files =
        memoryFiles({{"data/p/predicate/lucky.json", R"({"type":"minecraft:random_chance","chance":0.5})"}});
    const ResourceStack stack{
        {ResourceStack::Layer{.contents = std::make_shared<const PackContents>("p", files, std::vector<std::string>{},
                                                                               basalt::wire::common::Logger{"Test"}),
                              .filter = {}}}};
    const ItemTags tags = itemTagsOf(stack);
    const auto error = [&](std::string_view registry, std::string_view text) {
        return reloadableElementError(registry, text, stack, tags);
    };
    CHECK_FALSE(
        error("loot_table", R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:stone"}],
        "condition":"p:lucky"}]})")
            .has_value());
    CHECK(error("loot_table", R"({"pools":[{"rolls":1,"entries":[{"type":"p:odd"}]}]})").has_value());
    CHECK(error("loot_table", R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:stone"}],
        "condition":"p:missing"}]})")
              .has_value());
    CHECK(error("predicate", R"({"type":"p:nothing"})").has_value());
    CHECK(error("recipe", R"({"type":"p:machine"})").has_value());
    CHECK(error("advancement", R"({"criteria":{"a":{"trigger":"p:never"}}})") == "Unknown trigger: p:never");
    CHECK_FALSE(error("advancement", R"({"criteria":{"a":{"trigger":"minecraft:impossible"}}})").has_value());
}
