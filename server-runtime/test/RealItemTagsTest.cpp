#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/RealItemTags.hpp>

#include <doctest.h>

#include <map>
#include <set>
#include <string>

using basalt::wire::common::Identifier;
namespace tags = basalt::server::runtime::world::realItemTags;

namespace {

/// A jar of tag files held in memory, with the two callbacks the loader asks for.
struct FakeJar {
    std::map<std::string, std::string, std::less<>> entries;
    /// Listed but not readable, which is what a truncated archive looks like.
    std::set<std::string, std::less<>> unreadable;

    [[nodiscard]] tags::JarReader reader() const {
        return tags::JarReader{[this](std::string_view prefix) {
                                   std::vector<std::string> names;
                                   for (const auto& entry : entries) {
                                       if (entry.first.starts_with(prefix)) {
                                           names.push_back(entry.first);
                                       }
                                   }
                                   return names;
                               },
                               [this](std::string_view name) -> std::optional<std::string> {
                                   const auto found = entries.find(name);
                                   if (found == entries.end() || unreadable.contains(name)) {
                                       return std::nullopt;
                                   }
                                   return found->second;
                               }};
    }
};

[[nodiscard]] Identifier item(const std::string& path) { return Identifier::vanilla(path); }

[[nodiscard]] FakeJar twoTags() {
    return FakeJar{
        {{"data/minecraft/tags/item/logs.json", R"({"values": ["minecraft:oak_log", "minecraft:birch_log"]})"},
         {"data/minecraft/tags/item/burnable.json", R"({"values": ["#minecraft:logs", "minecraft:oak_planks"]})"}},
        {}};
}

} // namespace

TEST_CASE("a tag that names another tag comes out with both sets of items") {
    const tags::TagMembers loaded = tags::load(twoTags().reader());

    CHECK(loaded.size() == 2);
    CHECK(tags::membersOf(loaded, "minecraft:logs").size() == 2);
    CHECK(tags::membersOf(loaded, "minecraft:burnable").size() == 3);
    CHECK(tags::contains(loaded, "minecraft:burnable", item("oak_log")));
    CHECK(tags::contains(loaded, "minecraft:burnable", item("oak_planks")));
    CHECK_FALSE(tags::contains(loaded, "minecraft:logs", item("oak_planks")));
    CHECK_FALSE(tags::contains(loaded, "minecraft:burnable", item("stone")));
}

TEST_CASE("a tag nobody declared has no members and holds nothing") {
    const tags::TagMembers loaded = tags::load(twoTags().reader());

    CHECK(tags::membersOf(loaded, "minecraft:planks").empty());
    CHECK_FALSE(tags::contains(loaded, "minecraft:planks", item("stone")));
    CHECK(tags::membersOf(loaded, "not:a:name").empty());
    CHECK_FALSE(tags::contains(loaded, "not:a:name", item("stone")));
}

TEST_CASE("what is not a readable tag file is passed over, and the rest still load") {
    FakeJar jar = twoTags();
    jar.entries["data/minecraft/tags/item/logs.json.mcmeta"] = R"({"values": []})";
    jar.entries["data/minecraft/tags/item/broken.json"] = "not json at all";
    jar.entries["data/minecraft/tags/item/gone.json"] = R"({"values": []})";
    jar.unreadable.insert("data/minecraft/tags/item/gone.json");

    const tags::TagMembers loaded = tags::load(jar.reader());

    CHECK(loaded.size() == 2);
    CHECK(tags::membersOf(loaded, "minecraft:burnable").size() == 3);
}

TEST_CASE("a required item the registry does not know sinks the whole graph") {
    FakeJar jar = twoTags();
    jar.entries["data/minecraft/tags/item/wrong.json"] = R"({"values": ["minecraft:not_an_item"]})";

    CHECK(tags::load(jar.reader()).empty());

    jar.entries["data/minecraft/tags/item/wrong.json"] =
        R"({"values": [{"id": "minecraft:not_an_item", "required": false}]})";
    const tags::TagMembers loaded = tags::load(jar.reader());

    CHECK(loaded.size() == 3);
    CHECK(tags::membersOf(loaded, "minecraft:wrong").empty());
    CHECK(tags::membersOf(loaded, "minecraft:logs").size() == 2);
}

TEST_CASE("no jar at all, or half of one, reads as no tags rather than a crash") {
    CHECK(tags::load(tags::JarReader{}).empty());
    CHECK(tags::load(tags::JarReader{twoTags().reader().entriesUnder, {}}).empty());
    CHECK(tags::load(tags::JarReader{{}, twoTags().reader().readText}).empty());
}

TEST_CASE("an entry the listing hands back from outside the tag directory keeps its whole path") {
    const tags::JarReader jar{[](std::string_view) { return std::vector<std::string>{"elsewhere/logs.json"}; },
                              [](std::string_view) -> std::optional<std::string> {
                                  return std::string{R"({"values": ["minecraft:oak_log"]})"};
                              }};
    const tags::TagMembers loaded = tags::load(jar);

    CHECK(loaded.size() == 1);
    CHECK(tags::membersOf(loaded, "minecraft:elsewhere/logs").size() == 1);
}
