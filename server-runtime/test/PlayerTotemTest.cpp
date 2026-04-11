#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/player/PlayerTotem.hpp>

#include <doctest.h>

using basalt::server::registry::ConsumeEffect;
using basalt::server::runtime::player::deathProtectionOf;
using basalt::server::storage::StoredItemStack;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;

TEST_CASE("a totem protects with its own default, and a component gives anything its own effects") {
    const auto totem = deathProtectionOf(StoredItemStack{.id = "minecraft:totem_of_undying", .count = 1});
    REQUIRE(totem.has_value());
    // parity: `DeathProtection.TOTEM_OF_UNDYING`: clear, then regeneration, absorption, fire resistance.
    REQUIRE(totem->size() == 4);
    CHECK((*totem)[0].as<ConsumeEffect::ClearAll>() != nullptr);
    CHECK((*totem)[1].as<ConsumeEffect::Apply>()->durationTicks == 900);
    CHECK((*totem)[1].as<ConsumeEffect::Apply>()->amplifier == 1);
    CHECK(!deathProtectionOf(StoredItemStack{.id = "minecraft:stone", .count = 1}).has_value());

    NbtCompound speed;
    speed.putString("id", "minecraft:speed");
    speed.putInt("duration", 40);
    NbtList effects{NbtType::Compound};
    effects.push(NbtTag::ofCompound(speed));
    NbtCompound apply;
    apply.putString("type", "minecraft:apply_effects");
    apply.set("effects", NbtTag::ofList(std::move(effects)));
    NbtList deathEffects{NbtType::Compound};
    deathEffects.push(NbtTag::ofCompound(apply));
    NbtCompound protection;
    protection.set("death_effects", NbtTag::ofList(std::move(deathEffects)));
    StoredItemStack stone{.id = "minecraft:stone", .count = 1};
    stone.components.set("minecraft:death_protection", NbtTag::ofCompound(protection));
    const auto custom = deathProtectionOf(stone);
    REQUIRE(custom.has_value());
    REQUIRE(custom->size() == 1);
    CHECK(custom->front().as<ConsumeEffect::Apply>()->effect.path() == "speed");
}
