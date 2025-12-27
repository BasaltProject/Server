#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/entity/GeneratedEntityPresentation.hpp>

#include <doctest.h>

using basalt::server::runtime::entity::generatedEntityEquipment;
using basalt::server::runtime::entity::generatedEntityMetadata;
using basalt::server::runtime::entity::GeneratedEquipment;
using basalt::server::runtime::entity::RuntimeEntityMetadata;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::protocol::play::SetEntityDataPacket;
namespace play = basalt::wire::protocol::play;

namespace {

/// The entry at this index, if the metadata carries one.
[[nodiscard]] const SetEntityDataPacket::Entry* entryAt(const std::vector<SetEntityDataPacket::Entry>& entries,
                                                        std::uint8_t index) {
    for (const SetEntityDataPacket::Entry& entry : entries) {
        if (entry.index == index) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("a structure entity with nothing unusual about it needs no metadata at all") {
    // parity: vanilla's initial packet only carries fields that differ from the defaults.
    CHECK(generatedEntityMetadata("cow", NbtCompound{}).empty());
    CHECK(generatedEntityMetadata("armor_stand", NbtCompound{}).empty());
}

TEST_CASE("the shared fields are read off the saved tag") {
    NbtCompound data;
    data.putBoolean("Invisible", true);
    data.putBoolean("NoGravity", true);
    data.putShort("Air", 42);
    data.putFloat("Health", 3.5F);
    data.putBoolean("NoAI", true);

    const std::vector<SetEntityDataPacket::Entry> entries = generatedEntityMetadata("zombie", data);
    const SetEntityDataPacket::Entry* flags = entryAt(entries, 0);
    REQUIRE(flags != nullptr);
    CHECK(std::get<SetEntityDataPacket::ByteValue>(flags->value).value == 0x20);
    REQUIRE(entryAt(entries, 5) != nullptr);
    const SetEntityDataPacket::Entry* air = entryAt(entries, 1);
    REQUIRE(air != nullptr);
    CHECK(std::get<SetEntityDataPacket::IntValue>(air->value).value == 42);
    const SetEntityDataPacket::Entry* health = entryAt(entries, 9);
    REQUIRE(health != nullptr);
    CHECK(std::get<SetEntityDataPacket::FloatValue>(health->value).value == doctest::Approx(3.5));
    // parity: `Mob`'s own flags byte, which is where NoAI lives.
    REQUIRE(entryAt(entries, 15) != nullptr);
}

TEST_CASE("health is read whichever numeric tag the save wrote it as") {
    // A world imported from an older save writes it as a double, vanilla as a float.
    NbtCompound asDouble;
    asDouble.putDouble("Health", 6.0);
    const std::vector<SetEntityDataPacket::Entry> entries = generatedEntityMetadata("cow", asDouble);
    const SetEntityDataPacket::Entry* health = entryAt(entries, 9);
    REQUIRE(health != nullptr);
    CHECK(std::get<SetEntityDataPacket::FloatValue>(health->value).value == doctest::Approx(6.0));
}

TEST_CASE("a sheep's colour and its shearing share one byte") {
    NbtCompound data;
    data.putByte("Color", 4);
    data.putBoolean("Sheared", true);
    const std::vector<SetEntityDataPacket::Entry> entries = generatedEntityMetadata("sheep", data);
    const SetEntityDataPacket::Entry* wool = entryAt(entries, 18);
    REQUIRE(wool != nullptr);
    CHECK(std::get<SetEntityDataPacket::ByteValue>(wool->value).value == (4 | 0x10));
}

TEST_CASE("a panda's genes go where the client keeps bytes, not where it keeps counters") {
    // parity: `Panda` defines UNHAPPY_COUNTER, SNEEZE_COUNTER and EAT_COUNTER at 18, 19 and 20, so
    // its two genes are 21 and 22. A byte at 19 is an integer field to the client, and it answers
    // by dropping the connection, which is why this is pinned rather than left to reading.
    NbtCompound data;
    data.putString("MainGene", "weak");
    data.putString("HiddenGene", "brown");

    const std::vector<SetEntityDataPacket::Entry> entries = generatedEntityMetadata("panda", data);
    CHECK(entryAt(entries, 19) == nullptr);
    CHECK(entryAt(entries, 20) == nullptr);
    const SetEntityDataPacket::Entry* main = entryAt(entries, 21);
    REQUIRE(main != nullptr);
    CHECK(std::get<SetEntityDataPacket::ByteValue>(main->value).value == 5);
    const SetEntityDataPacket::Entry* hidden = entryAt(entries, 22);
    REQUIRE(hidden != nullptr);
    CHECK(std::get<SetEntityDataPacket::ByteValue>(hidden->value).value == 4);
}

TEST_CASE("a villager carries its trade and the flag that says it was chosen") {
    NbtCompound villager;
    villager.putString("type", "minecraft:taiga");
    villager.putString("profession", "minecraft:librarian");
    villager.putInt("level", 3);
    NbtCompound data;
    data.set("VillagerData", basalt::wire::nbt::NbtTag::ofCompound(villager));

    const std::vector<SetEntityDataPacket::Entry> entries = generatedEntityMetadata("villager", data);
    const SetEntityDataPacket::Entry* trade = entryAt(entries, 19);
    REQUIRE(trade != nullptr);
    const auto& value = std::get<SetEntityDataPacket::VillagerDataValue>(trade->value);
    CHECK(value.typeId == 6);
    CHECK(value.professionId == 9);
    CHECK(value.level == 3);
    // Loading explicit VillagerData also flips the finalized flag.
    REQUIRE(entryAt(entries, 20) != nullptr);
}

TEST_CASE("a cat that is black carries no variant, since black is the default") {
    NbtCompound black;
    black.putString("variant", "minecraft:black");
    const std::vector<SetEntityDataPacket::Entry> asBlack = generatedEntityMetadata("cat", black);
    CHECK(entryAt(asBlack, 20) == nullptr);

    NbtCompound calico;
    calico.putString("variant", "minecraft:calico");
    const std::vector<SetEntityDataPacket::Entry> entries = generatedEntityMetadata("cat", calico);
    const SetEntityDataPacket::Entry* variant = entryAt(entries, 20);
    REQUIRE(variant != nullptr);
    CHECK(std::get<SetEntityDataPacket::RegistryIdValue>(variant->value).value == 5);
}

TEST_CASE("what a generated entity is wearing comes back slot by slot") {
    NbtCompound sword;
    sword.putString("id", "minecraft:iron_sword");
    sword.putInt("count", 1);
    NbtCompound equipment;
    equipment.set("mainhand", basalt::wire::nbt::NbtTag::ofCompound(sword));
    NbtCompound data;
    data.set("equipment", basalt::wire::nbt::NbtTag::ofCompound(equipment));

    const std::vector<GeneratedEquipment> worn = generatedEntityEquipment(data);
    REQUIRE(worn.size() == 1);
    CHECK(worn.front().slot == play::SetEquipmentPacket::SlotType::MainHand);
    CHECK(worn.front().item == "minecraft:iron_sword");
    CHECK(worn.front().count == 1);

    // An empty slot is not something to send.
    CHECK(generatedEntityEquipment(NbtCompound{}).empty());
}

TEST_CASE("the live fields are sent once and then only when they change") {
    const RuntimeEntityMetadata fresh{.airSupply = 300, .baby = false};
    // Nothing here differs from the defaults, so the first packet carries nothing.
    CHECK(fresh.changesFrom(nullptr).empty());

    const RuntimeEntityMetadata drowning{.airSupply = 120, .baby = false};
    CHECK(drowning.changesFrom(nullptr).size() == 1);
    CHECK(drowning.changesFrom(&fresh).size() == 1);
    CHECK(drowning.changesFrom(&drowning).empty());

    const RuntimeEntityMetadata grown{.airSupply = 120, .baby = true};
    CHECK(grown.changesFrom(&drowning).size() == 1);
}
