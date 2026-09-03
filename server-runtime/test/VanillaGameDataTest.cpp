#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/VanillaGameData.hpp>

#include <doctest.h>

#include <map>
#include <string>
#include <vector>

using basalt::server::runtime::VanillaGameData;
using basalt::wire::common::Identifier;

namespace {

/// A jar that is a map of names to text, which is how every table here reaches a test.
struct TextJar {
    std::map<std::string, std::string> files;

    [[nodiscard]] VanillaGameData::Jar handle() const {
        return {
            .readText = [this](std::string_view name) -> std::optional<std::string> {
                const auto found = files.find(std::string{name});
                return found == files.end() ? std::nullopt : std::optional{found->second};
            },
            .entriesUnder =
                [this](std::string_view prefix) {
                    std::vector<std::string> names;
                    for (const auto& [name, text] : files) {
                        if (name.starts_with(prefix)) {
                            names.push_back(name);
                        }
                    }
                    return names;
                },
        };
    }
};

} // namespace

TEST_CASE("with no jar every table is empty rather than absent") {
    VanillaGameData data{VanillaGameData::jarOf(nullptr)};

    CHECK(data.enchantments.size() == 0);
    CHECK(data.dialogs.size() == 0);
    CHECK(data.testInstances.all().empty());
    CHECK(data.blockTags.empty());
    CHECK(data.timelineTags.empty());
    CHECK(data.structureTemplates.templateIds().empty());
    // Asking an empty table for something answers nothing, which is what a test that never breaks a
    // block in survival relies on.
    CHECK(data.lootTables.forBlock(Identifier::vanilla("stone")) == nullptr);
}

TEST_CASE("every table reads from the one jar it was given") {
    TextJar jar;
    jar.files["data/minecraft/enchantment/sharpness.json"] = R"({"anvil_cost":1})";
    jar.files["data/minecraft/dialog/server_links.json"] = R"({"type":"minecraft:server_links"})";
    jar.files["data/minecraft/loot_table/blocks/stone.json"] =
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:cobblestone"}]}]})";
    jar.files["data/minecraft/tags/block/mineable/pickaxe.json"] = R"({"values":["minecraft:stone"]})";
    VanillaGameData data{jar.handle()};

    CHECK(data.enchantments.contains(Identifier::vanilla("sharpness")));
    CHECK(data.dialogs.contains(Identifier::vanilla("server_links")));
    CHECK(data.lootTables.forBlock(Identifier::vanilla("stone")) != nullptr);
    CHECK(basalt::server::runtime::world::realBlockTags::membersOf(data.blockTags, "minecraft:mineable/pickaxe")
              .contains(Identifier::vanilla("stone")));
}
