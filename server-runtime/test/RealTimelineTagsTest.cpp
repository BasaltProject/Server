#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/RealTimelineTags.hpp>

#include <doctest.h>

#include <map>
#include <set>
#include <string>

using basalt::wire::common::Identifier;
namespace tags = basalt::server::runtime::world::realTimelineTags;

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

[[nodiscard]] Identifier timeline(const std::string& path) { return Identifier::vanilla(path); }

[[nodiscard]] FakeJar twoTags() {
    return FakeJar{
        {{"data/minecraft/tags/timeline/in_overworld.json", R"({"values": ["minecraft:overworld"]})"},
         {"data/minecraft/tags/timeline/all.json", R"({"values": ["#minecraft:in_overworld", "minecraft:the_end"]})"}},
        {}};
}

} // namespace

TEST_CASE("a tag that names another tag comes out with both sets of timelines") {
    const tags::TagMembers loaded = tags::load(twoTags().reader());

    CHECK(loaded.size() == 2);
    CHECK(tags::membersOf(loaded, timeline("in_overworld")).size() == 1);
    CHECK(tags::membersOf(loaded, timeline("all")).size() == 2);
    CHECK(tags::membersOf(loaded, timeline("all")).contains(timeline("overworld")));
    CHECK(tags::membersOf(loaded, timeline("all")).contains(timeline("the_end")));
    CHECK_FALSE(tags::membersOf(loaded, timeline("in_overworld")).contains(timeline("the_end")));
}

TEST_CASE("a timeline no registry lists is still taken as real, since the jar's own list is the list") {
    FakeJar jar = twoTags();
    jar.entries["data/minecraft/tags/timeline/odd.json"] = R"({"values": ["basalt:made_up"]})";

    const tags::TagMembers loaded = tags::load(jar.reader());

    CHECK(loaded.size() == 3);
    CHECK(tags::membersOf(loaded, timeline("odd")).contains(Identifier{"basalt", "made_up"}));
}

TEST_CASE("a tag nobody declared has no members") {
    const tags::TagMembers loaded = tags::load(twoTags().reader());

    CHECK(tags::membersOf(loaded, timeline("in_nether")).empty());
}

TEST_CASE("what is not a readable tag file is passed over, and the rest still load") {
    FakeJar jar = twoTags();
    jar.entries["data/minecraft/tags/timeline/all.json.mcmeta"] = R"({"values": []})";
    jar.entries["data/minecraft/tags/timeline/broken.json"] = "not json at all";
    jar.entries["data/minecraft/tags/timeline/gone.json"] = R"({"values": []})";
    jar.unreadable.insert("data/minecraft/tags/timeline/gone.json");

    const tags::TagMembers loaded = tags::load(jar.reader());

    CHECK(loaded.size() == 2);
    CHECK(tags::membersOf(loaded, timeline("all")).size() == 2);
}

TEST_CASE("a required reference to a tag nobody wrote leaves out that tag alone, as vanilla does") {
    FakeJar jar = twoTags();
    jar.entries["data/minecraft/tags/timeline/wrong.json"] = R"({"values": ["#minecraft:in_nether"]})";

    CHECK(tags::load(jar.reader()).size() == 2);

    jar.entries["data/minecraft/tags/timeline/wrong.json"] =
        R"({"values": [{"id": "#minecraft:in_nether", "required": false}]})";
    const tags::TagMembers loaded = tags::load(jar.reader());

    CHECK(loaded.size() == 3);
    CHECK(tags::membersOf(loaded, timeline("wrong")).empty());
    CHECK(tags::membersOf(loaded, timeline("all")).size() == 2);
}

TEST_CASE("no jar at all, or half of one, reads as no tags rather than a crash") {
    CHECK(tags::load(tags::JarReader{}).empty());
    CHECK(tags::load(tags::JarReader{twoTags().reader().entriesUnder, {}}).empty());
    CHECK(tags::load(tags::JarReader{{}, twoTags().reader().readText}).empty());
}
