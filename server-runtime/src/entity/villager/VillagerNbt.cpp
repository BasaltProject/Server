#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>
#include <basalt/server/runtime/entity/villager/VillagerNbt.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity::villager::villagerNbt {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr std::string_view kDefaultDimension = "minecraft:overworld";

[[nodiscard]] const NbtCompound* compoundAt(const NbtCompound& tag, std::string_view key) {
    return tag.getCompound(key);
}

void putPosition(NbtCompound& memories, std::string_view name, const Memory<BlockPos>& memory,
                 const VillagerState& state, std::size_t slotIndex) {
    std::optional<BlockPos> pos;
    std::string dimension;
    if (memory.present()) {
        pos = *memory.value;
        dimension = state.positionDimensions[slotIndex];
    } else if (state.foreignPositions[slotIndex].has_value()) {
        pos = state.foreignPositions[slotIndex]->pos;
        dimension = state.foreignPositions[slotIndex]->dimension;
    }
    if (!pos.has_value()) {
        return;
    }
    if (dimension.empty()) {
        dimension = state.dimension.empty() ? std::string{kDefaultDimension} : state.dimension;
    }
    NbtCompound value;
    value.putIntArray("pos", {pos->x, pos->y, pos->z});
    value.putString("dimension", std::move(dimension));
    NbtCompound slot;
    slot.set("value", NbtTag::ofCompound(std::move(value)));
    memories.set(name, NbtTag::ofCompound(std::move(slot)));
}

void putLong(NbtCompound& memories, std::string_view name, const Memory<std::int64_t>& memory) {
    if (!memory.present()) {
        return;
    }
    NbtCompound slot;
    slot.putLong("value", *memory.value);
    if (memory.ttl != kNoExpiry) {
        slot.putLong("ttl", memory.ttl);
    }
    memories.set(name, NbtTag::ofCompound(std::move(slot)));
}

void readPosition(const NbtCompound& memories, std::string_view name, Memory<BlockPos>& memory, VillagerState& state,
                  std::size_t slotIndex) {
    const NbtCompound* slot = compoundAt(memories, name);
    const NbtCompound* value = slot == nullptr ? nullptr : slot->getCompound("value");
    if (value == nullptr) {
        return;
    }
    const std::span<const std::int32_t> pos = value->getIntArray("pos");
    if (pos.size() == 3) {
        memory.set(BlockPos{pos[0], pos[1], pos[2]});
        state.positionDimensions[slotIndex] = std::string{value->getString("dimension", kDefaultDimension)};
    }
}

void readLong(const NbtCompound& memories, std::string_view name, Memory<std::int64_t>& memory) {
    const NbtCompound* slot = compoundAt(memories, name);
    if (slot == nullptr || !slot->contains("value")) {
        return;
    }
    memory.set(slot->getLong("value"), slot->contains("ttl") ? slot->getLong("ttl") : kNoExpiry);
}

[[nodiscard]] NbtCompound stackTag(const server::world::loot::ItemStack& stack) {
    NbtCompound tag;
    tag.putString("id", stack.item.toString());
    tag.putInt("count", stack.count);
    if (stack.components.size() > 0) {
        tag.set("components", NbtTag::ofCompound(stack.components));
    }
    return tag;
}

} // namespace

void write(const ServerEntity& villager, NbtCompound& tag) {
    if (villager.villager == nullptr) {
        return;
    }
    const VillagerState& state = *villager.villager;
    tag.putBoolean("VillagerDataFinalized", true);
    tag.putByte("FoodLevel", static_cast<std::int8_t>(state.foodLevel));
    tag.set("Gossips", NbtTag::ofList(state.gossips.encode()));
    tag.putLong("LastRestock", state.lastRestock);
    tag.putLong("LastGossipDecay", state.lastGossipDecay);
    tag.putInt("RestocksToday", state.restocksToday);
    // parity: `InventoryCarrier.writeInventoryToTag`, the non-empty stacks only.
    NbtList inventory{NbtType::Compound};
    for (const std::optional<server::world::loot::ItemStack>& slot : state.inventory) {
        if (slot.has_value() && slot->count > 0) {
            static_cast<void>(inventory.push(NbtTag::ofCompound(stackTag(*slot))));
        }
    }
    tag.set("Inventory", NbtTag::ofList(std::move(inventory)));

    NbtCompound brain;
    if (const NbtCompound* old = tag.getCompound("Brain"); old != nullptr) {
        brain = *old;
    }
    NbtCompound memories;
    const Memories& memory = state.memories;
    putPosition(memories, "minecraft:home", memory.home, state, 0);
    putPosition(memories, "minecraft:job_site", memory.jobSite, state, 1);
    putPosition(memories, "minecraft:potential_job_site", memory.potentialJobSite, state, 2);
    putPosition(memories, "minecraft:meeting_point", memory.meetingPoint, state, 3);
    putLong(memories, "minecraft:last_slept", memory.lastSlept);
    putLong(memories, "minecraft:last_woken", memory.lastWoken);
    putLong(memories, "minecraft:last_worked_at_poi", memory.lastWorkedAtPoi);
    if (memory.golemDetectedRecently.present()) {
        NbtCompound slot;
        slot.putBoolean("value", true);
        if (memory.golemDetectedRecently.ttl != kNoExpiry) {
            slot.putLong("ttl", memory.golemDetectedRecently.ttl);
        }
        memories.set("minecraft:golem_detected_recently", NbtTag::ofCompound(std::move(slot)));
    }
    brain.set("memories", NbtTag::ofCompound(std::move(memories)));
    tag.set("Brain", NbtTag::ofCompound(std::move(brain)));
}

void read(const NbtCompound& tag, ServerEntity& villager) {
    VillagerState& state = villagerBrain::stateOf(villager);
    state.dataFinalized = tag.getBoolean("VillagerDataFinalized") || tag.contains("VillagerData");
    state.foodLevel = tag.getByte("FoodLevel");
    if (const NbtList* gossips = tag.getList("Gossips"); gossips != nullptr) {
        state.gossips.decode(*gossips);
    }
    state.lastRestock = tag.getLong("LastRestock");
    state.lastGossipDecay = tag.getLong("LastGossipDecay");
    state.restocksToday = tag.getInt("RestocksToday");
    if (const NbtList* inventory = tag.getList("Inventory"); inventory != nullptr) {
        std::size_t slot = 0;
        for (const NbtTag& entry : inventory->elements()) {
            const NbtCompound* stack = entry.get<NbtCompound>();
            if (stack == nullptr || slot >= state.inventory.size()) {
                continue;
            }
            const auto id = wire::common::Identifier::parse(stack->getString("id"));
            if (!id.has_value()) {
                continue;
            }
            server::world::loot::ItemStack read{.item = *id, .count = stack->getInt("count", 1)};
            if (const NbtCompound* components = stack->getCompound("components"); components != nullptr) {
                read.components = *components;
            }
            state.inventory[slot++] = std::move(read);
        }
    }
    const NbtCompound* brain = tag.getCompound("Brain");
    const NbtCompound* memories = brain == nullptr ? nullptr : brain->getCompound("memories");
    if (memories == nullptr) {
        return;
    }
    Memories& memory = state.memories;
    readPosition(*memories, "minecraft:home", memory.home, state, 0);
    readPosition(*memories, "minecraft:job_site", memory.jobSite, state, 1);
    readPosition(*memories, "minecraft:potential_job_site", memory.potentialJobSite, state, 2);
    readPosition(*memories, "minecraft:meeting_point", memory.meetingPoint, state, 3);
    readLong(*memories, "minecraft:last_slept", memory.lastSlept);
    readLong(*memories, "minecraft:last_woken", memory.lastWoken);
    readLong(*memories, "minecraft:last_worked_at_poi", memory.lastWorkedAtPoi);
    if (const NbtCompound* slot = memories->getCompound("minecraft:golem_detected_recently");
        slot != nullptr && slot->getBoolean("value")) {
        memory.golemDetectedRecently.set(true, slot->contains("ttl") ? slot->getLong("ttl") : kNoExpiry);
    }
    // parity: `readAdditionalSaveData` ends in `refreshBrain`.
    state.refresh = true;
}

} // namespace basalt::server::runtime::entity::villager::villagerNbt
