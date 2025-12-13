#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>

#include <doctest.h>

#include <map>
#include <set>
#include <string>
#include <vector>

using basalt::wire::common::Identifier;
namespace tags = basalt::server::runtime::world::realDamageTypeTags;
namespace damageTypes = basalt::server::runtime::world::damageTypes;

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

[[nodiscard]] std::string tagPath(const std::string& name) {
    return "data/minecraft/tags/damage_type/" + name + ".json";
}

} // namespace

TEST_CASE("a tag lists the damage types the jar put in it") {
    FakeJar jar;
    jar.entries[tagPath("bypasses_armor")] =
        R"({"values": ["minecraft:starve", "minecraft:drown", "minecraft:wither"]})";

    const tags::TagMembers loaded = tags::load(jar.reader());

    CHECK(loaded.size() == 1);
    CHECK(tags::membersOf(loaded, tags::kBypassesArmour).size() == 3);
    CHECK(tags::isIn(loaded, tags::kBypassesArmour, damageTypes::kStarve));
    CHECK(tags::isIn(loaded, tags::kBypassesArmour, damageTypes::kDrown));
    CHECK_FALSE(tags::isIn(loaded, tags::kBypassesArmour, damageTypes::kFall));
}

TEST_CASE("a tag that names another takes everything in it") {
    FakeJar jar;
    jar.entries[tagPath("bypasses_armor")] = R"({"values": ["minecraft:starve", "#minecraft:always_hurts"]})";
    jar.entries[tagPath("always_hurts")] = R"({"values": ["minecraft:wither", "minecraft:magic"]})";

    const tags::TagMembers loaded = tags::load(jar.reader());

    CHECK(tags::membersOf(loaded, tags::kBypassesArmour).size() == 3);
    CHECK(tags::isIn(loaded, tags::kBypassesArmour, damageTypes::kWither));
    CHECK(tags::isIn(loaded, tags::kBypassesArmour, damageTypes::kMagic));
}

TEST_CASE("a damage type nobody has a registry for is still taken as real") {
    // parity aside: damage types are not a registry this build models, so an id it has never heard
    // of is kept rather than dropped, or a rule would go missing in silence.
    FakeJar jar;
    jar.entries[tagPath("bypasses_effects")] = R"({"values": ["minecraft:something_new"]})";

    const tags::TagMembers loaded = tags::load(jar.reader());

    CHECK(tags::isIn(loaded, tags::kBypassesEffects, Identifier::vanilla("something_new")));
}

TEST_CASE("a file that will not read or will not parse is skipped, and the rest still load") {
    FakeJar jar;
    jar.entries[tagPath("bypasses_armor")] = R"({"values": ["minecraft:starve"]})";
    jar.entries[tagPath("bypasses_effects")] = "{not json at all";
    jar.entries[tagPath("bypasses_resistance")] = R"({"values": ["minecraft:magic"]})";
    jar.unreadable.insert(tagPath("bypasses_resistance"));
    jar.entries["data/minecraft/tags/damage_type/notes.txt"] = "ignored";

    const tags::TagMembers loaded = tags::load(jar.reader());

    CHECK(loaded.size() == 1);
    CHECK(tags::isIn(loaded, tags::kBypassesArmour, damageTypes::kStarve));
    CHECK(tags::membersOf(loaded, tags::kBypassesEffects).empty());
    CHECK(tags::membersOf(loaded, tags::kBypassesResistance).empty());
}

TEST_CASE("a tag naming one that does not exist leaves the whole graph empty") {
    // Armour stopping everything is bad, and half a rule set is worse: the loader says so in the
    // log and hands back nothing.
    FakeJar jar;
    jar.entries[tagPath("bypasses_armor")] = R"({"values": ["#minecraft:nobody_wrote_this"]})";

    CHECK(tags::load(jar.reader()).empty());
}

TEST_CASE("a tag may name one that does not exist when it says it need not") {
    FakeJar jar;
    jar.entries[tagPath("bypasses_armor")] =
        R"({"values": ["minecraft:starve", {"id": "#minecraft:nobody_wrote_this", "required": false}]})";

    const tags::TagMembers loaded = tags::load(jar.reader());

    CHECK(tags::isIn(loaded, tags::kBypassesArmour, damageTypes::kStarve));
}

TEST_CASE("without a jar there are no tags at all") {
    CHECK(tags::load(tags::JarReader{}).empty());
    CHECK(tags::membersOf(tags::TagMembers{}, tags::kBypassesArmour).empty());
    CHECK_FALSE(tags::isIn(tags::TagMembers{}, tags::kBypassesArmour, damageTypes::kStarve));
}

TEST_CASE("the four tags that matter are named as the jar names them") {
    CHECK(tags::kBypassesArmour == Identifier::vanilla("bypasses_armor"));
    CHECK(tags::kBypassesEffects == Identifier::vanilla("bypasses_effects"));
    CHECK(tags::kBypassesResistance == Identifier::vanilla("bypasses_resistance"));
    CHECK(tags::kBypassesEnchantments == Identifier::vanilla("bypasses_enchantments"));
}
