#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/DamageProtection.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;

namespace {

[[nodiscard]] NbtCompound linearValue(double base, double perLevelAboveFirst) {
    NbtCompound value;
    value.putDouble("base", base);
    value.putDouble("per_level_above_first", perLevelAboveFirst);
    NbtCompound effect;
    effect.set("value", NbtTag::ofCompound(std::move(value)));
    return effect;
}

/// One `damage_protection` entry: what it is worth, and what the source has to be for it to count.
[[nodiscard]] NbtCompound entry(NbtCompound effect, std::optional<NbtCompound> requirements) {
    NbtCompound one;
    one.set("effect", NbtTag::ofCompound(std::move(effect)));
    if (requirements.has_value()) {
        one.set("requirements", NbtTag::ofCompound(std::move(*requirements)));
    }
    return one;
}

[[nodiscard]] NbtCompound definitionOf(std::vector<NbtCompound> entries) {
    NbtList list{NbtType::Compound};
    for (NbtCompound& one : entries) {
        REQUIRE(list.push(NbtTag::ofCompound(std::move(one))));
    }
    NbtCompound effects;
    effects.set("minecraft:damage_protection", NbtTag::ofList(std::move(list)));
    NbtCompound definition;
    definition.set("effects", NbtTag::ofCompound(std::move(effects)));
    return definition;
}

/// A `damage_source_properties` condition testing one tag, or one damage type with `direct`.
[[nodiscard]] NbtCompound tagCondition(const std::string& tag, bool expected, bool direct = false) {
    NbtCompound test;
    test.putString("id", direct ? tag : "#" + tag);
    test.putBoolean("expected", expected);
    NbtList tests{NbtType::Compound};
    REQUIRE(tests.push(NbtTag::ofCompound(std::move(test))));
    NbtCompound predicate;
    predicate.set("tags", NbtTag::ofList(std::move(tests)));
    NbtCompound condition;
    condition.putString("type", "minecraft:damage_source_properties");
    condition.set("predicate", NbtTag::ofCompound(std::move(predicate)));
    return condition;
}

[[nodiscard]] NbtCompound combined(const std::string& condition, std::vector<NbtCompound> terms) {
    NbtList list{NbtType::Compound};
    for (NbtCompound& term : terms) {
        REQUIRE(list.push(NbtTag::ofCompound(std::move(term))));
    }
    NbtCompound all;
    all.putString("type", condition);
    all.set("terms", NbtTag::ofList(std::move(list)));
    return all;
}

/// The world around one call: the enchantments each piece carries, the definitions and the tags.
struct World {
    std::map<Identifier, NbtCompound> definitions;
    std::map<Identifier, std::vector<Identifier>> tags;
    std::int32_t definitionsAsked = 0;

    [[nodiscard]] damageProtection::Sources sources() {
        damageProtection::Sources sources;
        sources.definitionOf = [this](const Identifier& enchantment) -> const NbtCompound* {
            ++definitionsAsked;
            const auto found = definitions.find(enchantment);
            return found == definitions.end() ? nullptr : &found->second;
        };
        sources.isDamageTypeIn = [this](const Identifier& tag, const Identifier& damageType) {
            const auto found = tags.find(tag);
            if (found == tags.end()) {
                return false;
            }
            return std::find(found->second.begin(), found->second.end(), damageType) != found->second.end();
        };
        return sources;
    }
};

using Worn = std::vector<damageProtection::EnchantmentLevels>;

const Identifier kProtection = Identifier::vanilla("protection");
const Identifier kBlastProtection = Identifier::vanilla("blast_protection");
const Identifier kExplosion = Identifier::vanilla("explosion");
const Identifier kFall = Identifier::vanilla("fall");

} // namespace

TEST_CASE("every piece and every level adds its own points") {
    World world;
    world.definitions.emplace(kProtection, definitionOf({entry(linearValue(1.0, 1.0), std::nullopt)}));
    const Worn worn = {{{kProtection, 1}}, {}, {}, {{kProtection, 4}}};
    // parity: `LevelBasedValue.Linear`, so level 1 is the base and level 4 is base plus three.
    CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(5.0F));
    CHECK(world.definitionsAsked == 2);
}

TEST_CASE("the linear value is the base plus one step for every level above the first") {
    World world;
    world.definitions.emplace(kProtection, definitionOf({entry(linearValue(2.5, 0.5), std::nullopt)}));
    for (const std::pair<std::int32_t, float> expected : {std::pair{1, 2.5F}, std::pair{2, 3.0F}, std::pair{5, 4.5F}}) {
        const Worn worn = {{{kProtection, expected.first}}};
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(expected.second));
    }
}

TEST_CASE("an effect with nothing to read is worth nothing") {
    World world;
    const Worn worn = {{{kProtection, 3}}};

    SUBCASE("an enchantment nobody defines") {
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(0.0F));
        CHECK(world.definitionsAsked == 1);
    }

    SUBCASE("a definition with no effects at all") {
        world.definitions.emplace(kProtection, NbtCompound{});
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(0.0F));
    }

    SUBCASE("effects that do not protect against damage") {
        NbtCompound effects;
        effects.putString("minecraft:damage_protection", "not a list");
        NbtCompound definition;
        definition.set("effects", NbtTag::ofCompound(std::move(effects)));
        world.definitions.emplace(kProtection, std::move(definition));
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(0.0F));
    }

    SUBCASE("a list of something that is not an entry") {
        NbtList strings{NbtType::String};
        REQUIRE(strings.push(NbtTag::ofString("nonsense")));
        NbtCompound effects;
        effects.set("minecraft:damage_protection", NbtTag::ofList(std::move(strings)));
        NbtCompound definition;
        definition.set("effects", NbtTag::ofCompound(std::move(effects)));
        world.definitions.emplace(kProtection, std::move(definition));
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(0.0F));
    }

    SUBCASE("an entry whose effect has no value") {
        world.definitions.emplace(kProtection, definitionOf({entry(NbtCompound{}, std::nullopt)}));
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(0.0F));
    }

    SUBCASE("nothing worn at all") {
        const Worn nothing = {{}, {}};
        world.definitions.emplace(kProtection, definitionOf({entry(linearValue(4.0, 0.0), std::nullopt)}));
        CHECK(damageProtection::of(nothing, &kExplosion, world.sources()) == doctest::Approx(0.0F));
        CHECK(world.definitionsAsked == 0);
    }

    SUBCASE("a piece with no enchantments on it") {
        const Worn bare = {{}};
        world.definitions.emplace(kProtection, definitionOf({entry(linearValue(4.0, 0.0), std::nullopt)}));
        CHECK(damageProtection::of(bare, &kExplosion, world.sources()) == doctest::Approx(0.0F));
        CHECK(world.definitionsAsked == 0);
    }
}

TEST_CASE("a requirement on the damage source decides whether the entry counts") {
    World world;
    world.tags.emplace(Identifier::vanilla("is_explosion"), std::vector{kExplosion});
    const Worn worn = {{{kBlastProtection, 1}}};

    SUBCASE("the source is in the tag it has to be in") {
        world.definitions.emplace(kBlastProtection,
                                  definitionOf({entry(linearValue(3.0, 0.0), tagCondition("is_explosion", true))}));
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(3.0F));
        CHECK(damageProtection::of(worn, &kFall, world.sources()) == doctest::Approx(0.0F));
    }

    SUBCASE("the source has to stay out of the tag") {
        world.definitions.emplace(kBlastProtection,
                                  definitionOf({entry(linearValue(3.0, 0.0), tagCondition("is_explosion", false))}));
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(0.0F));
        CHECK(damageProtection::of(worn, &kFall, world.sources()) == doctest::Approx(3.0F));
    }

    SUBCASE("a hit with no type at all fails every tag test") {
        world.definitions.emplace(kBlastProtection,
                                  definitionOf({entry(linearValue(3.0, 0.0), tagCondition("is_explosion", false))}));
        CHECK(damageProtection::of(worn, nullptr, world.sources()) == doctest::Approx(0.0F));
    }

    SUBCASE("a predicate that tests nothing lets a typeless hit through") {
        NbtCompound condition;
        condition.putString("type", "minecraft:damage_source_properties");
        condition.set("predicate", NbtTag::ofCompound(NbtCompound{}));
        world.definitions.emplace(kBlastProtection, definitionOf({entry(linearValue(3.0, 0.0), std::move(condition))}));
        CHECK(damageProtection::of(worn, nullptr, world.sources()) == doctest::Approx(3.0F));
    }

    SUBCASE("a condition with no predicate at all is met") {
        NbtCompound condition;
        condition.putString("type", "minecraft:damage_source_properties");
        world.definitions.emplace(kBlastProtection, definitionOf({entry(linearValue(3.0, 0.0), std::move(condition))}));
        CHECK(damageProtection::of(worn, &kFall, world.sources()) == doctest::Approx(3.0F));
    }

    SUBCASE("a condition nobody knows is unmet") {
        NbtCompound condition;
        condition.putString("type", "minecraft:random_chance");
        world.definitions.emplace(kBlastProtection, definitionOf({entry(linearValue(3.0, 0.0), std::move(condition))}));
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(0.0F));
    }

    SUBCASE("a requirement with no condition named is unmet") {
        world.definitions.emplace(kBlastProtection, definitionOf({entry(linearValue(3.0, 0.0), NbtCompound{})}));
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(0.0F));
    }

    SUBCASE("a test naming something that is not an identifier is skipped") {
        NbtCompound test;
        test.putString("id", "a:b:c");
        test.putBoolean("expected", true);
        NbtList tests{NbtType::Compound};
        REQUIRE(tests.push(NbtTag::ofCompound(std::move(test))));
        NbtCompound predicate;
        predicate.set("tags", NbtTag::ofList(std::move(tests)));
        NbtCompound condition;
        condition.putString("type", "minecraft:damage_source_properties");
        condition.set("predicate", NbtTag::ofCompound(std::move(predicate)));
        world.definitions.emplace(kBlastProtection, definitionOf({entry(linearValue(3.0, 0.0), std::move(condition))}));
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(3.0F));
    }

    SUBCASE("a test naming one damage type holds for that type alone") {
        world.definitions.emplace(
            kBlastProtection, definitionOf({entry(linearValue(3.0, 0.0), tagCondition(kFall.toString(), true, true))}));
        CHECK(damageProtection::of(worn, &kFall, world.sources()) == doctest::Approx(3.0F));
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(0.0F));
    }

    SUBCASE("a test with no name is skipped") {
        NbtCompound test;
        test.putInt("id", 5);
        test.putBoolean("expected", true);
        NbtList tests{NbtType::Compound};
        REQUIRE(tests.push(NbtTag::ofCompound(std::move(test))));
        NbtCompound predicate;
        predicate.set("tags", NbtTag::ofList(std::move(tests)));
        NbtCompound condition;
        condition.putString("type", "minecraft:damage_source_properties");
        condition.set("predicate", NbtTag::ofCompound(std::move(predicate)));
        world.definitions.emplace(kBlastProtection, definitionOf({entry(linearValue(3.0, 0.0), std::move(condition))}));
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(3.0F));
    }
}

TEST_CASE("all_of and any_of walk their terms") {
    World world;
    world.tags.emplace(Identifier::vanilla("is_explosion"), std::vector{kExplosion});
    world.tags.emplace(Identifier::vanilla("is_fall"), std::vector{kFall});
    const Worn worn = {{{kBlastProtection, 1}}};

    SUBCASE("all_of needs every term") {
        NbtCompound condition =
            combined("minecraft:all_of", {tagCondition("is_explosion", true), tagCondition("is_fall", false)});
        world.definitions.emplace(kBlastProtection, definitionOf({entry(linearValue(3.0, 0.0), std::move(condition))}));
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(3.0F));
        CHECK(damageProtection::of(worn, &kFall, world.sources()) == doctest::Approx(0.0F));
    }

    SUBCASE("any_of needs one term") {
        NbtCompound condition =
            combined("minecraft:any_of", {tagCondition("is_explosion", true), tagCondition("is_fall", true)});
        world.definitions.emplace(kBlastProtection, definitionOf({entry(linearValue(3.0, 0.0), std::move(condition))}));
        CHECK(damageProtection::of(worn, &kFall, world.sources()) == doctest::Approx(3.0F));
        const Identifier magic = Identifier::vanilla("magic");
        CHECK(damageProtection::of(worn, &magic, world.sources()) == doctest::Approx(0.0F));
    }

    SUBCASE("no terms at all: all_of and any_of both hold") {
        NbtCompound allOf;
        allOf.putString("type", "minecraft:all_of");
        NbtCompound anyOf;
        anyOf.putString("type", "minecraft:any_of");
        world.definitions.emplace(kBlastProtection, definitionOf({entry(linearValue(1.0, 0.0), std::move(allOf)),
                                                                  entry(linearValue(2.0, 0.0), std::move(anyOf))}));
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(3.0F));
    }

    SUBCASE("an empty term list: all_of holds and any_of does not") {
        world.definitions.emplace(kBlastProtection,
                                  definitionOf({entry(linearValue(1.0, 0.0), combined("minecraft:all_of", {})),
                                                entry(linearValue(2.0, 0.0), combined("minecraft:any_of", {}))}));
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(1.0F));
    }

    SUBCASE("terms that are not conditions are passed over") {
        NbtList strings{NbtType::String};
        REQUIRE(strings.push(NbtTag::ofString("nonsense")));
        NbtCompound allOf;
        allOf.putString("type", "minecraft:all_of");
        allOf.set("terms", NbtTag::ofList(NbtList{strings}));
        NbtCompound anyOf;
        anyOf.putString("type", "minecraft:any_of");
        anyOf.set("terms", NbtTag::ofList(std::move(strings)));
        world.definitions.emplace(kBlastProtection, definitionOf({entry(linearValue(1.0, 0.0), std::move(allOf)),
                                                                  entry(linearValue(2.0, 0.0), std::move(anyOf))}));
        CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(1.0F));
    }
}

TEST_CASE("more than one entry on the same enchantment adds up, and each is judged on its own") {
    World world;
    world.tags.emplace(Identifier::vanilla("is_explosion"), std::vector{kExplosion});

    world.definitions.emplace(kBlastProtection,
                              definitionOf({entry(linearValue(1.0, 1.0), std::nullopt),
                                            entry(linearValue(0.5, 0.0), tagCondition("is_explosion", true)),
                                            entry(linearValue(8.0, 0.0), tagCondition("is_explosion", false))}));
    const Worn worn = {{{kBlastProtection, 2}}};

    CHECK(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(2.5F));
    CHECK(damageProtection::of(worn, &kFall, world.sources()) == doctest::Approx(10.0F));
}

TEST_CASE("a set of sources with a piece missing answers nothing") {
    World world;
    world.definitions.emplace(kProtection, definitionOf({entry(linearValue(4.0, 0.0), std::nullopt)}));
    const Worn worn = {{{kProtection, 1}}};
    REQUIRE(damageProtection::of(worn, &kExplosion, world.sources()) == doctest::Approx(4.0F));

    damageProtection::Sources withoutDefinitions = world.sources();
    withoutDefinitions.definitionOf = {};
    CHECK(damageProtection::of(worn, &kExplosion, withoutDefinitions) == doctest::Approx(0.0F));

    damageProtection::Sources withoutTags = world.sources();
    withoutTags.isDamageTypeIn = {};
    CHECK(damageProtection::of(worn, &kExplosion, withoutTags) == doctest::Approx(0.0F));
}
