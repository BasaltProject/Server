#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/item/ItemComponentPayloads.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace codecs = basalt::server::runtime::item::itemComponentCodecs;
namespace payloads = basalt::server::runtime::item::itemComponentPayloads;
namespace reg = basalt::server::registry;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
using basalt::wire::protocol::play::ItemComponent;
namespace dataComponent = basalt::wire::protocol::play::dataComponent;

namespace {

[[nodiscard]] std::optional<std::int32_t> enchantmentId(const Identifier& name) {
    if (name.path() == "sharpness") {
        return 12;
    }
    if (name.path() == "unbreaking") {
        return 30;
    }
    return std::nullopt;
}

[[nodiscard]] const Identifier* enchantmentName(std::int32_t id) {
    static const Identifier sharpness = Identifier::vanilla("sharpness");
    static const Identifier unbreaking = Identifier::vanilla("unbreaking");
    if (id == 12) {
        return &sharpness;
    }
    return id == 30 ? &unbreaking : nullptr;
}

[[nodiscard]] NbtCompound compoundOf(const std::map<std::string, std::int32_t>& levels) {
    NbtCompound compound;
    for (const auto& [name, level] : levels) {
        compound.putInt(name, level);
    }
    return compound;
}

} // namespace

TEST_CASE("a number component travels as its number") {
    const std::optional<ItemComponent> damage = codecs::toWire("minecraft:damage", NbtTag::ofInt(12), enchantmentId);

    REQUIRE(damage.has_value());
    CHECK(damage->id() == dataComponent::kDamage);
    REQUIRE(damage->as<ItemComponent::Number>() != nullptr);
    CHECK(damage->as<ItemComponent::Number>()->value == 12);
}

TEST_CASE("a whole stack's components go over in the order they were stored") {
    NbtCompound components;
    components.putInt("minecraft:damage", 5);
    components.putString("minecraft:rarity", "epic");
    components.putInt("minecraft:not_a_component", 1);

    const std::vector<ItemComponent> wire = codecs::toWire(components, enchantmentId);

    REQUIRE(wire.size() == 2);
    CHECK(wire[0].id() == dataComponent::kDamage);
    CHECK(wire[1].id() == dataComponent::kRarity);
}

TEST_CASE("what goes out comes back as what was stored") {
    NbtCompound components;
    components.putInt("minecraft:damage", 5);
    components.putString("minecraft:rarity", "uncommon");
    components.set("minecraft:enchantments", NbtTag::ofCompound(compoundOf({{"minecraft:sharpness", 4}})));
    components.set("minecraft:unbreakable", NbtTag::ofCompound(NbtCompound{}));
    components.set("minecraft:custom_name",
                   basalt::wire::text::componentNbt::serialize(basalt::wire::text::Component::text("Sting")));
    components.putInt("minecraft:dyed_color", 255);

    const NbtCompound back = codecs::toStored(codecs::toWire(components, enchantmentId), enchantmentName);

    CHECK(back == components);
}

TEST_CASE("a component written straight into bytes carries its own payload") {
    const std::optional<ItemComponent> map = payloads::encode("minecraft:map_id", NbtTag::ofInt(7));

    REQUIRE(map.has_value());
    const auto* raw = map->as<ItemComponent::Raw>();
    REQUIRE(raw != nullptr);
    REQUIRE(raw->payload.size() == 1);
    CHECK(raw->payload[0] == 7);
    CHECK(map->id() == reg::dataComponents::idOf(Identifier::vanilla("map_id")).value());
}

TEST_CASE("a holder set is a tag name or a list of ids") {
    NbtCompound repairable;
    repairable.putString("items", "#minecraft:planks");
    const std::optional<ItemComponent> tagged =
        payloads::encode("minecraft:repairable", NbtTag::ofCompound(repairable));
    REQUIRE(tagged.has_value());
    // parity: a zero, then the tag name without its hash.
    const std::vector<std::uint8_t>& payload = tagged->as<ItemComponent::Raw>()->payload;
    REQUIRE(payload.size() > 2);
    CHECK(payload[0] == 0);
    CHECK(payload[1] == std::string_view{"minecraft:planks"}.size());

    repairable.putString("items", "minecraft:stick");
    const std::optional<ItemComponent> single =
        payloads::encode("minecraft:repairable", NbtTag::ofCompound(repairable));
    REQUIRE(single.has_value());
    // parity: a two, meaning one entry, and then the item's own id.
    CHECK(single->as<ItemComponent::Raw>()->payload[0] == 2);

    repairable.putString("items", "minecraft:not_an_item");
    CHECK_FALSE(payloads::encode("minecraft:repairable", NbtTag::ofCompound(repairable)).has_value());
}
