#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>

#include <doctest.h>

#include <map>
#include <set>
#include <string>

using basalt::wire::common::Identifier;
namespace tags = basalt::server::runtime::world::realEntityTypeTags;

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

[[nodiscard]] Identifier type(const std::string& path) { return Identifier::vanilla(path); }

[[nodiscard]] FakeJar twoTags() {
    return FakeJar{
        {{"data/minecraft/tags/entity_type/zombies.json", R"({"values": ["minecraft:zombie", "minecraft:husk"]})"},
         {"data/minecraft/tags/entity_type/monsters.json",
          R"({"values": ["#minecraft:zombies", "minecraft:creeper"]})"}},
        {}};
}

} // namespace

TEST_CASE("a tag that names another tag comes out with both sets of types") {
    const tags::TagMembers loaded = tags::load(twoTags().reader());

    CHECK(loaded.size() == 2);
    CHECK(tags::membersOf(loaded, type("zombies")).size() == 2);
    CHECK(tags::membersOf(loaded, type("monsters")).size() == 3);
    CHECK(tags::membersOf(loaded, type("monsters")).contains(type("husk")));
    CHECK(tags::membersOf(loaded, type("monsters")).contains(type("creeper")));
    CHECK_FALSE(tags::membersOf(loaded, type("zombies")).contains(type("creeper")));
}

TEST_CASE("a tag nobody declared has no members") {
    const tags::TagMembers loaded = tags::load(twoTags().reader());

    CHECK(tags::membersOf(loaded, type("raiders")).empty());
}

TEST_CASE("what is not a readable tag file is passed over, and the rest still load") {
    FakeJar jar = twoTags();
    jar.entries["data/minecraft/tags/entity_type/zombies.json.mcmeta"] = R"({"values": []})";
    jar.entries["data/minecraft/tags/entity_type/broken.json"] = "not json at all";
    jar.entries["data/minecraft/tags/entity_type/gone.json"] = R"({"values": []})";
    jar.unreadable.insert("data/minecraft/tags/entity_type/gone.json");

    const tags::TagMembers loaded = tags::load(jar.reader());

    CHECK(loaded.size() == 2);
    CHECK(tags::membersOf(loaded, type("monsters")).size() == 3);
}

TEST_CASE("a required type the registry does not know leaves out that tag alone, as vanilla does") {
    FakeJar jar = twoTags();
    jar.entries["data/minecraft/tags/entity_type/wrong.json"] = R"({"values": ["minecraft:not_a_mob"]})";

    CHECK(tags::load(jar.reader()).size() == 2);

    jar.entries["data/minecraft/tags/entity_type/wrong.json"] =
        R"({"values": [{"id": "minecraft:not_a_mob", "required": false}]})";
    const tags::TagMembers loaded = tags::load(jar.reader());

    CHECK(loaded.size() == 3);
    CHECK(tags::membersOf(loaded, type("wrong")).empty());
    CHECK(tags::membersOf(loaded, type("zombies")).size() == 2);
}

TEST_CASE("no jar at all, or half of one, reads as no tags rather than a crash") {
    CHECK(tags::load(tags::JarReader{}).empty());
    CHECK(tags::load(tags::JarReader{twoTags().reader().entriesUnder, {}}).empty());
    CHECK(tags::load(tags::JarReader{{}, twoTags().reader().readText}).empty());
}
