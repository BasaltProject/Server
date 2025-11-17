#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/RealFluidTags.hpp>

#include <doctest.h>

#include <map>
#include <set>
#include <string>

using basalt::wire::common::Identifier;
namespace tags = basalt::server::runtime::world::realFluidTags;

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

[[nodiscard]] Identifier fluid(const std::string& path) { return Identifier::vanilla(path); }

[[nodiscard]] FakeJar twoTags() {
    return FakeJar{
        {{"data/minecraft/tags/fluid/water.json", R"({"values": ["minecraft:water", "minecraft:flowing_water"]})"},
         {"data/minecraft/tags/fluid/wet.json", R"({"values": ["#minecraft:water", "minecraft:empty"]})"}},
        {}};
}

} // namespace

TEST_CASE("the five fluids the protocol knows travel as their own numbers") {
    CHECK(tags::protocolIds().size() == 5);
    CHECK(tags::protocolIds().at(fluid("empty")) == 0);
    CHECK(tags::protocolIds().at(fluid("flowing_water")) == 1);
    CHECK(tags::protocolIds().at(fluid("water")) == 2);
    CHECK(tags::protocolIds().at(fluid("flowing_lava")) == 3);
    CHECK(tags::protocolIds().at(fluid("lava")) == 4);
    CHECK_FALSE(tags::protocolIds().contains(fluid("milk")));
}

TEST_CASE("a tag that names another tag comes out with both sets of fluids") {
    const tags::TagMembers loaded = tags::load(twoTags().reader());

    CHECK(loaded.size() == 2);
    CHECK(tags::membersOf(loaded, fluid("water")).size() == 2);
    CHECK(tags::membersOf(loaded, fluid("wet")).size() == 3);
    CHECK(tags::membersOf(loaded, fluid("wet")).contains(fluid("flowing_water")));
    CHECK(tags::membersOf(loaded, fluid("wet")).contains(fluid("empty")));
    CHECK_FALSE(tags::membersOf(loaded, fluid("water")).contains(fluid("empty")));
}

TEST_CASE("a tag nobody declared has no members") {
    const tags::TagMembers loaded = tags::load(twoTags().reader());

    CHECK(tags::membersOf(loaded, fluid("lava")).empty());
}

TEST_CASE("what is not a readable tag file is passed over, and the rest still load") {
    FakeJar jar = twoTags();
    jar.entries["data/minecraft/tags/fluid/water.json.mcmeta"] = R"({"values": []})";
    jar.entries["data/minecraft/tags/fluid/broken.json"] = "not json at all";
    jar.entries["data/minecraft/tags/fluid/gone.json"] = R"({"values": []})";
    jar.unreadable.insert("data/minecraft/tags/fluid/gone.json");

    const tags::TagMembers loaded = tags::load(jar.reader());

    CHECK(loaded.size() == 2);
    CHECK(tags::membersOf(loaded, fluid("wet")).size() == 3);
}

TEST_CASE("a required fluid outside the five sinks the whole graph") {
    FakeJar jar = twoTags();
    jar.entries["data/minecraft/tags/fluid/wrong.json"] = R"({"values": ["minecraft:milk"]})";

    CHECK(tags::load(jar.reader()).empty());

    jar.entries["data/minecraft/tags/fluid/wrong.json"] =
        R"({"values": [{"id": "minecraft:milk", "required": false}]})";
    const tags::TagMembers loaded = tags::load(jar.reader());

    CHECK(loaded.size() == 3);
    CHECK(tags::membersOf(loaded, fluid("wrong")).empty());
    CHECK(tags::membersOf(loaded, fluid("water")).size() == 2);
}

TEST_CASE("no jar at all, or half of one, reads as no tags rather than a crash") {
    CHECK(tags::load(tags::JarReader{}).empty());
    CHECK(tags::load(tags::JarReader{twoTags().reader().entriesUnder, {}}).empty());
    CHECK(tags::load(tags::JarReader{{}, twoTags().reader().readText}).empty());
}

TEST_CASE("an entry the listing hands back from outside the tag directory keeps its whole path") {
    const tags::JarReader jar{[](std::string_view) { return std::vector<std::string>{"elsewhere/water.json"}; },
                              [](std::string_view) -> std::optional<std::string> {
                                  return std::string{R"({"values": ["minecraft:water"]})"};
                              }};
    const tags::TagMembers loaded = tags::load(jar);

    CHECK(loaded.size() == 1);
    CHECK(tags::membersOf(loaded, fluid("elsewhere/water")).size() == 1);
}
