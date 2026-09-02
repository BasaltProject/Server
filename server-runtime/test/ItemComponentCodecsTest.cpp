#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/item/ItemComponentPayloads.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <doctest.h>

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

/// A registry of two enchantments, which is all any of these need.
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

TEST_CASE("a byte where a number belongs is still a number, and a string is not") {
    const std::optional<ItemComponent> fromByte =
        codecs::toWire("minecraft:repair_cost", NbtTag::ofByte(3), enchantmentId);
    REQUIRE(fromByte.has_value());
    CHECK(fromByte->as<ItemComponent::Number>()->value == 3);

    CHECK_FALSE(codecs::toWire("minecraft:damage", NbtTag::ofString("twelve"), enchantmentId).has_value());
    CHECK_FALSE(codecs::toWire("minecraft:damage", NbtTag::ofFloat(1.5F), enchantmentId).has_value());
}

TEST_CASE("a component that carries nothing but itself travels as a unit") {
    const std::optional<ItemComponent> unbreakable =
        codecs::toWire("minecraft:unbreakable", NbtTag::ofCompound(NbtCompound{}), enchantmentId);

    REQUIRE(unbreakable.has_value());
    CHECK(unbreakable->id() == dataComponent::kUnbreakable);
    CHECK(unbreakable->as<ItemComponent::Unit>() != nullptr);
}

TEST_CASE("a name travels as the text it is") {
    const NbtTag stored = basalt::wire::text::componentNbt::serialize(basalt::wire::text::Component::text("Excalibur"));

    const std::optional<ItemComponent> named = codecs::toWire("minecraft:custom_name", stored, enchantmentId);

    REQUIRE(named.has_value());
    CHECK(named->id() == dataComponent::kCustomName);
    REQUIRE(named->as<ItemComponent::Text>() != nullptr);
    CHECK(named->as<ItemComponent::Text>()->text == basalt::wire::text::Component::text("Excalibur"));
}

TEST_CASE("enchantments travel by registry id, and the ones nobody knows are dropped") {
    const NbtTag levels = NbtTag::ofCompound(compoundOf({{"minecraft:sharpness", 5}, {"minecraft:nonsense", 2}}));

    const std::optional<ItemComponent> enchanted = codecs::toWire("minecraft:enchantments", levels, enchantmentId);

    REQUIRE(enchanted.has_value());
    CHECK(enchanted->id() == dataComponent::kEnchantments);
    const auto* payload = enchanted->as<ItemComponent::Enchantments>();
    REQUIRE(payload != nullptr);
    CHECK_FALSE(payload->stored);
    REQUIRE(payload->levels.size() == 1);
    CHECK(payload->levels[0].first == 12);
    CHECK(payload->levels[0].second == 5);
}

TEST_CASE("a book's enchantments say so, and a set nobody can resolve is not sent at all") {
    const NbtTag levels = NbtTag::ofCompound(compoundOf({{"minecraft:unbreaking", 3}}));

    const std::optional<ItemComponent> book = codecs::toWire("minecraft:stored_enchantments", levels, enchantmentId);
    REQUIRE(book.has_value());
    CHECK(book->id() == dataComponent::kStoredEnchantments);
    CHECK(book->as<ItemComponent::Enchantments>()->stored);

    const NbtTag unknown = NbtTag::ofCompound(compoundOf({{"minecraft:nonsense", 1}}));
    CHECK_FALSE(codecs::toWire("minecraft:enchantments", unknown, enchantmentId).has_value());

    // Not a compound at all is not an enchantment set either.
    CHECK_FALSE(codecs::toWire("minecraft:enchantments", NbtTag::ofInt(1), enchantmentId).has_value());
}

TEST_CASE("rarity travels as its place in vanilla's own order") {
    const std::optional<ItemComponent> rare =
        codecs::toWire("minecraft:rarity", NbtTag::ofString("rare"), enchantmentId);

    REQUIRE(rare.has_value());
    CHECK(rare->id() == dataComponent::kRarity);
    CHECK(rare->as<ItemComponent::Number>()->value == 2);

    CHECK(codecs::toWire("minecraft:rarity", NbtTag::ofString("common"), enchantmentId)
              ->as<ItemComponent::Number>()
              ->value == 0);
    CHECK_FALSE(codecs::toWire("minecraft:rarity", NbtTag::ofString("legendary"), enchantmentId).has_value());
}

TEST_CASE("lore travels as a list of lines and a colour as its number") {
    NbtList lines{NbtType::Compound};
    lines.push(basalt::wire::text::componentNbt::serialize(basalt::wire::text::Component::text("one")));
    lines.push(basalt::wire::text::componentNbt::serialize(basalt::wire::text::Component::text("two")));

    const std::optional<ItemComponent> lore =
        codecs::toWire("minecraft:lore", NbtTag::ofList(std::move(lines)), enchantmentId);
    REQUIRE(lore.has_value());
    REQUIRE(lore->as<ItemComponent::Lore>() != nullptr);
    CHECK(lore->as<ItemComponent::Lore>()->lines.size() == 2);

    const std::optional<ItemComponent> dyed =
        codecs::toWire("minecraft:dyed_color", NbtTag::ofInt(0x336699), enchantmentId);
    REQUIRE(dyed.has_value());
    CHECK(dyed->as<ItemComponent::Color>()->rgb == 0x336699);
}

TEST_CASE("the glint override is the only boolean component") {
    const std::optional<ItemComponent> on =
        codecs::toWire("minecraft:enchantment_glint_override", NbtTag::ofByte(1), enchantmentId);
    REQUIRE(on.has_value());
    CHECK(on->as<ItemComponent::Flag>()->value);

    const std::optional<ItemComponent> off =
        codecs::toWire("minecraft:enchantment_glint_override", NbtTag::ofByte(0), enchantmentId);
    REQUIRE(off.has_value());
    CHECK_FALSE(off->as<ItemComponent::Flag>()->value);
}

TEST_CASE("custom data travels as the compound it is") {
    NbtCompound tag;
    tag.putString("owner", "someone");

    const std::optional<ItemComponent> custom =
        codecs::toWire("minecraft:custom_data", NbtTag::ofCompound(tag), enchantmentId);

    REQUIRE(custom.has_value());
    CHECK(custom->as<ItemComponent::CustomData>()->tag == tag);
    CHECK_FALSE(codecs::toWire("minecraft:custom_data", NbtTag::ofInt(1), enchantmentId).has_value());
}

TEST_CASE("a component with no codec anywhere reaches no client") {
    CHECK_FALSE(codecs::toWire("minecraft:not_a_component", NbtTag::ofInt(1), enchantmentId).has_value());
    CHECK_FALSE(codecs::isSent("minecraft:not_a_component"));
    CHECK_FALSE(codecs::exists("minecraft:not_a_component"));
    CHECK(codecs::exists("minecraft:damage"));
    CHECK(codecs::exists("minecraft:attribute_modifiers"));
}

TEST_CASE("the components a client will not see are named in the order they were written") {
    NbtCompound components;
    components.putInt("minecraft:damage", 1);
    components.putInt("minecraft:attribute_modifiers", 0);
    components.putInt("minecraft:map_id", 4);
    components.putInt("minecraft:not_a_component", 0);

    const std::vector<std::string> missing = codecs::notDrawn(components);

    REQUIRE(missing.size() == 2);
    CHECK(missing[0] == "minecraft:attribute_modifiers");
    CHECK(missing[1] == "minecraft:not_a_component");
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

TEST_CASE("a component the client echoed back that this build does not model is dropped") {
    const ItemComponent raw{dataComponent::kDamage, ItemComponent::Raw{.payload = {1, 2, 3}}};

    CHECK_FALSE(codecs::toStored(raw, enchantmentName).has_value());

    // And one whose id belongs to no component at all.
    const ItemComponent nowhere{999999, ItemComponent::Number{.value = 1}};
    CHECK_FALSE(codecs::toStored(nowhere, enchantmentName).has_value());
}

TEST_CASE("an enchantment set that resolves to nothing is not stored either") {
    const ItemComponent enchanted{dataComponent::kEnchantments,
                                  ItemComponent::Enchantments{.levels = {{404, 1}}, .stored = false}};

    CHECK_FALSE(codecs::toStored(enchanted, enchantmentName).has_value());
}

TEST_CASE("a rarity outside vanilla's four is not stored") {
    const ItemComponent rarity{dataComponent::kRarity, ItemComponent::Number{.value = 9}};

    CHECK_FALSE(codecs::toStored(rarity, enchantmentName).has_value());
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

TEST_CASE("a payload whose value is the wrong shape is refused rather than half written") {
    NbtCompound food;
    food.putFloat("saturation", 1.0F);

    // parity: `FoodProperties` has no default for its nutrition, so a value without one is not one.
    CHECK_FALSE(payloads::encode("minecraft:food", NbtTag::ofCompound(food)).has_value());

    food.putInt("nutrition", 4);
    const std::optional<ItemComponent> whole = payloads::encode("minecraft:food", NbtTag::ofCompound(food));
    REQUIRE(whole.has_value());
    // One varint, one float and one boolean.
    CHECK(whole->as<ItemComponent::Raw>()->payload.size() == 6);
}

TEST_CASE("an enum payload is its own ordinal, and a word nobody knows is refused") {
    const std::optional<ItemComponent> dye = payloads::encode("minecraft:dye", NbtTag::ofString("cyan"));

    REQUIRE(dye.has_value());
    REQUIRE(dye->as<ItemComponent::Raw>()->payload.size() == 1);
    CHECK(dye->as<ItemComponent::Raw>()->payload[0] == 9);

    CHECK_FALSE(payloads::encode("minecraft:dye", NbtTag::ofString("puce")).has_value());
    CHECK_FALSE(payloads::encode("minecraft:dye", NbtTag::ofInt(9)).has_value());
}

TEST_CASE("a component with no payload of its own writes nothing at all") {
    const std::optional<ItemComponent> glider = payloads::encode("minecraft:glider", NbtTag::ofCompound(NbtCompound{}));

    REQUIRE(glider.has_value());
    CHECK(glider->as<ItemComponent::Raw>()->payload.empty());
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

TEST_CASE("everything the payload table can write is reported as sent") {
    CHECK(payloads::supported().contains("minecraft:tool"));
    CHECK(codecs::isSent("minecraft:tool"));
    CHECK(codecs::isSent("minecraft:damage"));
    CHECK_FALSE(payloads::supported().contains("minecraft:damage"));
}
