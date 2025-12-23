#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/entity/Attributes.hpp>
#include <basalt/server/runtime/world/MobSpawnAttributes.hpp>
#include <basalt/server/runtime/world/MobSpawnVariantData.hpp>
#include <basalt/server/runtime/world/WeaponDamage.hpp>

#include <doctest.h>

#include <map>
#include <string>
#include <vector>

using basalt::server::random::RandomSource;
using basalt::server::runtime::entity::AttributeOperation;
using basalt::server::runtime::world::PandaGene;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
namespace attributes = basalt::server::runtime::world::mobSpawnAttributes;
namespace enchantability = basalt::server::runtime::world::vanillaItemEnchantability;
namespace nbt = basalt::server::runtime::world::spawnAttributeNbt;
namespace variants = basalt::server::runtime::world::mobSpawnVariantData;
namespace weaponDamage = basalt::server::runtime::world::weaponDamage;

namespace {

[[nodiscard]] const NbtCompound* attributeNamed(const NbtCompound& data, std::string_view id) {
    const NbtTag* found = data.find("attributes");
    const auto* list = found == nullptr ? nullptr : found->get<NbtList>();
    if (list == nullptr) {
        return nullptr;
    }
    for (const NbtTag& entry : list->elements()) {
        const auto* compound = entry.get<NbtCompound>();
        const NbtTag* name = compound == nullptr ? nullptr : compound->find("id");
        const auto* text = name == nullptr ? nullptr : name->get<std::string>();
        if (text != nullptr && *text == id) {
            return compound;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("a horse rolls all three of its attributes and a mule only its health") {
    RandomSource random = RandomSource::legacy(42);
    std::map<std::string, double> rolled;
    attributes::write("horse", random, NbtCompound{}, rolled);
    CHECK(rolled.size() == 3);
    // parity: fifteen plus two rolls, so the range is 15 to 30 and never outside it.
    CHECK(rolled.at("minecraft:max_health") >= 15.0);
    CHECK(rolled.at("minecraft:max_health") <= 30.0);
    CHECK(rolled.contains("minecraft:jump_strength"));

    rolled.clear();
    attributes::write("mule", random, NbtCompound{}, rolled);
    CHECK(rolled.size() == 1);
    CHECK(rolled.contains("minecraft:max_health"));

    // A type with nothing to roll is left entirely alone rather than given defaults.
    rolled.clear();
    attributes::write("cow", random, NbtCompound{}, rolled);
    CHECK(rolled.empty());
}

TEST_CASE("a recessive panda gene only shows when both copies agree") {
    std::map<std::string, double> rolled;
    NbtCompound mixed;
    mixed.set("MainGene", NbtTag::ofString("weak"));
    mixed.set("HiddenGene", NbtTag::ofString("normal"));
    RandomSource random = RandomSource::legacy(1);
    attributes::write("panda", random, mixed, rolled);
    CHECK(rolled.empty());

    NbtCompound both;
    both.set("MainGene", NbtTag::ofString("weak"));
    both.set("HiddenGene", NbtTag::ofString("weak"));
    attributes::write("panda", random, both, rolled);
    CHECK(rolled.at("minecraft:max_health") == 10.0);

    // Lazy is dominant, so one copy is enough for the slower walk.
    rolled.clear();
    NbtCompound lazy;
    lazy.set("MainGene", NbtTag::ofString("lazy"));
    lazy.set("HiddenGene", NbtTag::ofString("normal"));
    attributes::write("panda", random, lazy, rolled);
    CHECK(rolled.contains("minecraft:movement_speed"));

    CHECK(attributes::pandaGene(0) == PandaGene::Lazy);
    CHECK(attributes::pandaGene(3) == PandaGene::Weak);
    CHECK(attributes::pandaGene(9) == PandaGene::Brown);
    CHECK(attributes::pandaGene(11) == PandaGene::Normal);
}

TEST_CASE("an attribute is written once and its modifiers replaced by id") {
    NbtCompound data;
    nbt::setBase(data, "minecraft:max_health", 24.0);
    nbt::setBase(data, "minecraft:max_health", 30.0);
    const NbtCompound* health = attributeNamed(data, "minecraft:max_health");
    REQUIRE(health != nullptr);
    CHECK(*health->find("base")->get<double>() == 30.0);

    nbt::addModifier(data, "minecraft:max_health", 30.0, "minecraft:zombie_bonus", 2.0, AttributeOperation::AddValue);
    nbt::addModifier(data, "minecraft:max_health", 30.0, "minecraft:zombie_bonus", 5.0, AttributeOperation::AddValue);
    health = attributeNamed(data, "minecraft:max_health");
    REQUIRE(health != nullptr);
    const auto* modifiers = health->find("modifiers")->get<NbtList>();
    REQUIRE(modifiers != nullptr);
    // Same id twice is one modifier with the newer amount, not two that both apply.
    REQUIRE(modifiers->size() == 1);
    CHECK(*(*modifiers)[0].get<NbtCompound>()->find("amount")->get<double>() == 5.0);
}

TEST_CASE("a damage enchantment adds what its own definition says, and a conditional one adds nothing") {
    const auto levelValue = [](double base, double perLevel) {
        NbtCompound value;
        value.set("base", NbtTag::ofDouble(base));
        value.set("per_level_above_first", NbtTag::ofDouble(perLevel));
        return value;
    };
    const auto damageEffect = [&levelValue](double base, double perLevel, bool conditional) {
        NbtCompound effect;
        effect.set("type", NbtTag::ofString("minecraft:add"));
        effect.set("value", NbtTag::ofCompound(levelValue(base, perLevel)));
        NbtCompound entry;
        entry.set("effect", NbtTag::ofCompound(effect));
        if (conditional) {
            entry.set("requirements", NbtTag::ofCompound(NbtCompound{}));
        }
        NbtCompound effects;
        effects.set("minecraft:damage",
                    NbtTag::ofList(NbtList{basalt::wire::nbt::NbtType::Compound, {NbtTag::ofCompound(entry)}}));
        NbtCompound definition;
        definition.set("effects", NbtTag::ofCompound(effects));
        return definition;
    };
    // parity: sharpness is 1 at level one and half a point per level after it.
    const NbtCompound sharpness = damageEffect(1.0, 0.5, false);
    const NbtCompound smite = damageEffect(2.5, 2.5, true);

    NbtCompound levels;
    levels.putInt("minecraft:sharpness", 3);
    levels.putInt("minecraft:smite", 5);
    NbtCompound components;
    components.set("minecraft:enchantments", NbtTag::ofCompound(levels));
    const basalt::server::storage::StoredItemStack sword{
        .id = "minecraft:diamond_sword", .count = 1, .components = components};

    const float damage = weaponDamage::of(
        sword, 7.0F, [&sharpness, &smite](const basalt::wire::common::Identifier& id) -> const NbtCompound* {
            if (id.path() == "sharpness") {
                return &sharpness;
            }
            return id.path() == "smite" ? &smite : nullptr;
        });
    // 7 plus sharpness III (1 + 0.5 * 2), and nothing at all from the conditional smite.
    CHECK(damage == 9.0F);

    // No stack at all is the base damage, unchanged.
    CHECK(weaponDamage::of(std::nullopt, 7.0F, [](const basalt::wire::common::Identifier&) -> const NbtCompound* {
              return nullptr;
          }) == 7.0F);
}

TEST_CASE("enchantability follows the item's material, and unrelated items have none") {
    CHECK(enchantability::valueOf("minecraft:golden_sword") == 22);
    CHECK(enchantability::valueOf("golden_helmet") == 25);
    CHECK(enchantability::valueOf("minecraft:turtle_helmet") == 9);
    CHECK(enchantability::valueOf("minecraft:book") == 1);
    // A stone that is not a tool, and a material that no armour is made of.
    CHECK_FALSE(enchantability::valueOf("minecraft:stone").has_value());
    CHECK_FALSE(enchantability::valueOf("minecraft:emerald_sword").has_value());
}

TEST_CASE("the tropical fish table packs its variants the way the entity's own field does") {
    const std::vector<std::int32_t>& all = variants::tropicalCommonVariants();
    REQUIRE(all.size() == 22);
    // The first is pattern 7, base 1, colour 7: pattern 7 is a large body and pattern 1 within it.
    CHECK((all[0] & 1) == 1);
    CHECK(((all[0] >> 8) & 0xFF) == 1);
    CHECK(((all[0] >> 16) & 0xFF) == 1);
    CHECK(((all[0] >> 24) & 0xFF) == 7);

    CHECK(variants::wolfVariants().size() == 9);
    CHECK(variants::warmFrogBiomes().contains("jungle"));
    CHECK(variants::coldFrogBiomes().contains("snowy_taiga"));
    CHECK_FALSE(variants::coldFrogBiomes().contains("jungle"));
}
