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
    built.slots[35] = StoredItemStack{.id = "minecraft:torch", .count = 12, .components = {}};
    built.slots[36] = StoredItemStack{.id = "minecraft:iron_helmet", .count = 1, .components = {}};
    built.slots[39] = StoredItemStack{.id = "minecraft:iron_boots", .count = 1, .components = {}};
    built.slots[40] = StoredItemStack{.id = "minecraft:shield", .count = 1, .components = {}};
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
