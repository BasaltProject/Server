#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/storage/StorageNbt.hpp>
#include <basalt/server/storage/VanillaPlayerData.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <doctest.h>

#include <cstdint>
#include <format>
#include <string>
#include <vector>

using namespace basalt::server::storage;
namespace nbt = basalt::wire::nbt;

namespace {

[[nodiscard]] std::int64_t checksumOf(std::span<const std::uint8_t> bytes) {
    std::uint64_t checksum = 14695981039346656037ULL;
    for (const std::uint8_t byte : bytes) {
        checksum = (checksum ^ static_cast<std::uint64_t>(byte)) * 1099511628211ULL;
    }
    return static_cast<std::int64_t>(checksum);
}

/// The oracle joins every slot, empty ones as nothing, then collapses the runs of separators. A
/// leading one survives that, which is why an all-empty prefix shows up as a single `;`.
[[nodiscard]] std::string describe(const StoredInventory& inventory) {
    std::string joined;
    for (std::size_t index = 0; index < inventory.slots.size(); ++index) {
        if (index != 0) {
            joined += ';';
        }
        const std::optional<StoredItemStack>& stack = inventory.slots[index];
        if (stack.has_value()) {
            joined += std::to_string(index) + "=" + stack->id + "x" + std::to_string(stack->count) +
                      nbt::snbt::stringify(nbt::NbtTag::ofCompound(stack->components));
        }
    }
    std::string collapsed;
    for (const char character : joined) {
        if (character == ';' && !collapsed.empty() && collapsed.back() == ';') {
            continue;
        }
        collapsed += character;
    }
    return collapsed;
}

[[nodiscard]] StoredInventory inventory() {
    StoredInventory built = StoredInventory::empty();
    built.slots[0] = StoredItemStack{.id = "minecraft:stone", .count = 64, .components = {}};
    nbt::NbtCompound named;
    named.putString("minecraft:custom_name", "Biter");
    built.slots[8] = StoredItemStack{.id = "minecraft:diamond_sword", .count = 1, .components = std::move(named)};
    // The last main slot, the four armour slots and the offhand: the ends of every range.
    built.slots[35] = StoredItemStack{.id = "minecraft:torch", .count = 12, .components = {}};
    built.slots[36] = StoredItemStack{.id = "minecraft:iron_helmet", .count = 1, .components = {}};
    built.slots[39] = StoredItemStack{.id = "minecraft:iron_boots", .count = 1, .components = {}};
    built.slots[40] = StoredItemStack{.id = "minecraft:shield", .count = 1, .components = {}};
    return built;
}

[[nodiscard]] nbt::NbtCompound item(std::int8_t slot, std::string_view id, std::int32_t count) {
    nbt::NbtCompound built;
    built.putByte("Slot", slot);
    built.putString("id", std::string{id});
    built.putInt("count", count);
    return built;
}

} // namespace

TEST_CASE("the inventory blob is the bytes the frozen kotlin writes") {
    const std::vector<std::uint8_t> encoded = inventoryNbt::encode(inventory());
    CHECK(encoded.size() == 383);
    CHECK(checksumOf(encoded) == -5502867550757164006LL);

    const std::expected<StoredInventory, StorageFormatError> back = inventoryNbt::decode(encoded);
    REQUIRE(back.has_value());
    CHECK(describe(*back) == "0=minecraft:stonex64{};8=minecraft:diamond_swordx1{\"minecraft:custom_name\":\"Biter\"};"
                             "35=minecraft:torchx12{};36=minecraft:iron_helmetx1{};39=minecraft:iron_bootsx1{};"
                             "40=minecraft:shieldx1{}");

    const std::vector<std::uint8_t> bare = inventoryNbt::encode(StoredInventory::empty(StoredPlayer::kEnderChestSlots));
    CHECK(bare.size() == 42);
    CHECK(checksumOf(bare) == -5251239502202668516LL);
    const std::expected<StoredInventory, StorageFormatError> emptyBack = inventoryNbt::decode(bare);
    REQUIRE(emptyBack.has_value());
    CHECK(emptyBack->slots.size() == 27);
    CHECK(emptyBack->isEmpty());
}

TEST_CASE("the stats blob is the bytes the frozen kotlin writes") {
    const StoredStats built = StoredStats{}
                                  .with(stats::deaths(), 3)
                                  .with(stats::walkOneCm(), 12345678901LL)
                                  .with(stats::mined("minecraft:deepslate"), 91)
                                  // Added then taken back to zero, which is dropped rather than written.
                                  .with(stats::jump(), 7)
                                  .with(stats::jump(), -7);
    const std::vector<std::uint8_t> encoded = statsNbt::encode(built);
    CHECK(encoded.size() == 169);
    CHECK(checksumOf(encoded) == 4845356069799846470LL);

    const std::expected<StoredStats, StorageFormatError> back = statsNbt::decode(encoded);
    REQUIRE(back.has_value());
    std::string counters;
    for (const auto& [key, value] : back->counters) {
        if (!counters.empty()) {
            counters += ',';
        }
        counters += key + "=" + std::to_string(value);
    }
    CHECK(counters == "minecraft:custom/minecraft:deaths=3,minecraft:custom/minecraft:walk_one_cm=12345678901,"
                      "minecraft:mined/minecraft:deepslate=91");
    CHECK(back->get(stats::deaths()) == 3);
    CHECK(back->get(stats::jump()) == 0);
    CHECK(back->get("minecraft:custom/minecraft:nothing") == 0);
    CHECK(statsNbt::encode(StoredStats{}).empty());
}

TEST_CASE("a blob this build cannot read is refused, not misread") {
    const auto refusal = [](const std::vector<std::uint8_t>& bytes) {
        const std::expected<StoredInventory, StorageFormatError> decoded = inventoryNbt::decode(bytes);
        return decoded.has_value() ? std::string{"accepted"} : decoded.error().message;
    };

    // The reader's own detail is not repeated here, so this message is shorter than the Kotlin's.
    CHECK(refusal({9, 9, 9}) == "inventory blob is not readable NBT");

    nbt::NbtCompound wrongVersion;
    wrongVersion.putInt("Version", 99);
    wrongVersion.putInt("Size", 4);
    CHECK(refusal(nbt::nbtIO::write(nbt::NbtTag::ofCompound(wrongVersion))) ==
          "inventory blob is format version 99, this build reads 1");

    nbt::NbtCompound badSlot;
    badSlot.putInt("Version", 1);
    badSlot.putInt("Size", 4);
    nbt::NbtCompound outOfRange;
    outOfRange.putInt("Slot", 9);
    nbt::NbtList slotList{nbt::NbtType::Compound};
    slotList.push(nbt::NbtTag::ofCompound(std::move(outOfRange)));
    badSlot.set("Slots", nbt::NbtTag::ofList(std::move(slotList)));
    CHECK(refusal(nbt::nbtIO::write(nbt::NbtTag::ofCompound(badSlot))) == "inventory blob has an item in slot 9 of 4");

    // A slot exactly at the size, which is one past the last valid index.
    nbt::NbtCompound slotAtSize;
    slotAtSize.putInt("Version", 1);
    slotAtSize.putInt("Size", 4);
    nbt::NbtCompound atSize;
    atSize.putInt("Slot", 4);
    nbt::NbtList atSizeList{nbt::NbtType::Compound};
    atSizeList.push(nbt::NbtTag::ofCompound(std::move(atSize)));
    slotAtSize.set("Slots", nbt::NbtTag::ofList(std::move(atSizeList)));
    CHECK(refusal(nbt::nbtIO::write(nbt::NbtTag::ofCompound(slotAtSize))) ==
          "inventory blob has an item in slot 4 of 4");

    nbt::NbtCompound badCount;
    badCount.putInt("Version", 1);
    badCount.putInt("Size", 4);
    nbt::NbtCompound emptyStack;
    emptyStack.putInt("Slot", 1);
    emptyStack.putInt("count", 0);
    nbt::NbtList countList{nbt::NbtType::Compound};
    countList.push(nbt::NbtTag::ofCompound(std::move(emptyStack)));
    badCount.set("Slots", nbt::NbtTag::ofList(std::move(countList)));
    CHECK(refusal(nbt::nbtIO::write(nbt::NbtTag::ofCompound(badCount))) == "inventory blob has 0 items in slot 1");
}

TEST_CASE("a vanilla playerdata file maps the way the frozen kotlin maps it") {
    // Built here rather than taken from a real world: nothing of Mojang's ships in this repository.
    nbt::NbtCompound vanilla;
    nbt::NbtList position{nbt::NbtType::Double};
    position.push(nbt::NbtTag::ofDouble(120.5));
    position.push(nbt::NbtTag::ofDouble(71.0));
    position.push(nbt::NbtTag::ofDouble(-340.25));
    vanilla.set("Pos", nbt::NbtTag::ofList(std::move(position)));
    nbt::NbtList rotation{nbt::NbtType::Float};
    rotation.push(nbt::NbtTag::ofFloat(-90.5F));
    rotation.push(nbt::NbtTag::ofFloat(12.25F));
    vanilla.set("Rotation", nbt::NbtTag::ofList(std::move(rotation)));
    vanilla.putFloat("Health", 17.5F);
    vanilla.putInt("foodLevel", 14);
    vanilla.putFloat("foodSaturationLevel", 2.5F);
    vanilla.putFloat("foodExhaustionLevel", 0.75F);
    vanilla.putInt("XpLevel", 30);
    vanilla.putFloat("XpP", 0.4F);
    vanilla.putInt("XpTotal", 1395);
    vanilla.putInt("playerGameType", 1);

    nbt::NbtList inventoryList{nbt::NbtType::Compound};
    inventoryList.push(nbt::NbtTag::ofCompound(item(0, "minecraft:dirt", 32)));
    // Armour and the offhand, at vanilla's own out-of-range slot numbers.
    inventoryList.push(nbt::NbtTag::ofCompound(item(100, "minecraft:leather_boots", 1)));
    inventoryList.push(nbt::NbtTag::ofCompound(item(103, "minecraft:iron_helmet", 1)));
    inventoryList.push(nbt::NbtTag::ofCompound(item(-106, "minecraft:shield", 1)));
    // Two slots just past the ends of vanilla's own two ranges, which belong to neither and are
    // skipped rather than folded into the armour or the offhand.
    inventoryList.push(nbt::NbtTag::ofCompound(item(36, "minecraft:bedrock", 1)));
    inventoryList.push(nbt::NbtTag::ofCompound(item(104, "minecraft:barrier", 1)));
    // A slot vanilla never uses and an empty stack: both are skipped.
    inventoryList.push(nbt::NbtTag::ofCompound(item(50, "minecraft:stone", 1)));
    inventoryList.push(nbt::NbtTag::ofCompound(item(5, "minecraft:air", 0)));
    vanilla.set("Inventory", nbt::NbtTag::ofList(std::move(inventoryList)));

    nbt::NbtList enderList{nbt::NbtType::Compound};
    enderList.push(nbt::NbtTag::ofCompound(item(26, "minecraft:ender_pearl", 16)));
    vanilla.set("EnderItems", nbt::NbtTag::ofList(std::move(enderList)));

    const std::vector<std::uint8_t> plain = nbt::nbtIO::write(nbt::NbtTag::ofCompound(std::move(vanilla)));
    const std::expected<StoredPlayer, StorageFormatError> imported = vanillaPlayerData::read(
        "b1e0c0de-0000-4000-8000-00000000abcd", "Steve", plain,
        [](std::span<const std::uint8_t> bytes) { return std::vector<std::uint8_t>{bytes.begin(), bytes.end()}; },
        1700000000LL, "world");
    REQUIRE(imported.has_value());

    CHECK(std::format("{}/{}/{}", imported->x, imported->y, imported->z) == "120.5/71/-340.25");
    CHECK(std::format("{}/{}", imported->yaw, imported->pitch) == "-90.5/12.25");
    CHECK(imported->health == 17.5F);
    CHECK(imported->food == 14);
    CHECK(imported->saturation == 2.5F);
    CHECK(imported->exhaustion == 0.75F);
    CHECK(imported->xpLevel == 30);
    CHECK(imported->xpProgress == 0.4F);
    CHECK(imported->xpTotal == 1395);
    CHECK(imported->gamemode == StoredGamemode::Creative);
    CHECK(imported->world == "world");
    CHECK(imported->playTimeTicks == 0);
    CHECK(imported->lastSeenEpochSeconds == 1700000000LL);
    CHECK(imported->firstSeenEpochSeconds == 1700000000LL);
    CHECK(describe(imported->inventory) ==
          "0=minecraft:dirtx32{};36=minecraft:leather_bootsx1{};39=minecraft:iron_helmetx1{};"
          "40=minecraft:shieldx1{}");
    CHECK(describe(imported->enderChest) == ";26=minecraft:ender_pearlx16{}");
}

TEST_CASE("an unknown gamemode id is survival, not a failure") {
    std::string modes;
    static constexpr std::string_view kNames[] = {"SURVIVAL", "CREATIVE", "ADVENTURE", "SPECTATOR"};
    for (std::int32_t id = -1; id <= 4; ++id) {
        if (!modes.empty()) {
            modes += ',';
        }
        modes += kNames[static_cast<std::size_t>(gamemodeById(id))];
    }
    CHECK(modes == "SURVIVAL,SURVIVAL,CREATIVE,ADVENTURE,SPECTATOR,SURVIVAL");
}
