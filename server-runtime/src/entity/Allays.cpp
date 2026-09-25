#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/entity/Allays.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>

#include <algorithm>
#include <memory>
#include <span>
#include <string_view>

namespace basalt::server::runtime::entity::allays {
namespace {

using server::world::loot::ItemStack;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr std::string_view kLikedPlayer = "minecraft:liked_player";
constexpr std::string_view kLikedNoteblock = "minecraft:liked_noteblock";
constexpr std::string_view kNoteblockCooldownMemory = "minecraft:liked_noteblock_cooldown_ticks";
constexpr std::string_view kDefaultDimension = "minecraft:overworld";

[[nodiscard]] NbtTag uuidTag(const wire::common::Uuid& uuid) {
    return NbtTag::ofIntArray({static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.high() >> 32)),
                               static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.high())),
                               static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.low() >> 32)),
                               static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.low()))});
}

[[nodiscard]] std::optional<wire::common::Uuid> uuidIn(const NbtCompound& tag, std::string_view key) {
    const std::span<const std::int32_t> parts = tag.getIntArray(key);
    if (parts.size() != 4) {
        return std::nullopt;
    }
    const auto word = [parts](std::size_t at) {
        return static_cast<std::uint64_t>(static_cast<std::uint32_t>(parts[at]));
    };
    return wire::common::Uuid{(word(0) << 32) | word(1), (word(2) << 32) | word(3)};
}

[[nodiscard]] std::int32_t maxStackOf(const ItemStack& stack) {
    if (const NbtTag* max = stack.components.find("minecraft:max_stack_size"); max != nullptr) {
        if (const std::int32_t* value = max->get<std::int32_t>(); value != nullptr) {
            return *value;
        }
    }
    return server::registry::itemStacking::maxStackSize(stack.item);
}

} // namespace

bool isAllay(const wire::common::Identifier& type) { return type == wire::common::Identifier::vanilla("allay"); }

AllayState& stateOf(ServerEntity& allay) {
    if (allay.allay != nullptr) {
        return *allay.allay;
    }
    allay.allay = std::make_shared<AllayState>();
    AllayState& state = *allay.allay;
    if (!allay.sourceNbt.has_value()) {
        return state;
    }
    const NbtCompound& tag = *allay.sourceNbt;
    if (const NbtList* inventory = tag.getList("Inventory"); inventory != nullptr) {
        for (const NbtTag& entry : inventory->elements()) {
            if (const NbtCompound* stack = entry.get<NbtCompound>(); stack != nullptr) {
                state.inventory = world::blockInventory::readStack(*stack);
                break;
            }
        }
    }
    // parity: saved as a long and read back with `getIntOr`, which takes any number.
    if (const NbtTag* cooldown = tag.find("DuplicationCooldown"); cooldown != nullptr) {
        if (const std::int64_t* wide = cooldown->get<std::int64_t>(); wide != nullptr) {
            state.duplicationCooldown = static_cast<std::int32_t>(*wide);
        } else if (const std::int32_t* narrow = cooldown->get<std::int32_t>(); narrow != nullptr) {
            state.duplicationCooldown = *narrow;
        }
    }
    const NbtCompound* brain = tag.getCompound("Brain");
    const NbtCompound* memories = brain == nullptr ? nullptr : brain->getCompound("memories");
    if (memories == nullptr) {
        return state;
    }
    if (const NbtCompound* liked = memories->getCompound(kLikedPlayer); liked != nullptr) {
        state.likedPlayer = uuidIn(*liked, "value");
    }
    if (const NbtCompound* noteblock = memories->getCompound(kLikedNoteblock); noteblock != nullptr) {
        if (const NbtCompound* value = noteblock->getCompound("value"); value != nullptr) {
            if (const std::span<const std::int32_t> pos = value->getIntArray("pos"); pos.size() == 3) {
                state.likedNoteblock = Pos{pos[0], pos[1], pos[2]};
                state.noteblockDimension = std::string{value->getString("dimension", kDefaultDimension)};
            }
        }
    }
    if (const NbtCompound* cooldown = memories->getCompound(kNoteblockCooldownMemory); cooldown != nullptr) {
        state.noteblockCooldown = cooldown->getInt("value");
    }
    return state;
}

void write(const ServerEntity& allay, NbtCompound& tag) {
    if (allay.allay == nullptr) {
        return;
    }
    const AllayState& state = *allay.allay;
    std::vector<NbtTag> items;
    if (state.inventory.has_value() && state.inventory->count > 0) {
        items.push_back(NbtTag::ofCompound(world::blockInventory::writeStack(*state.inventory)));
    }
    tag.set("Inventory", NbtTag::ofList(NbtList{NbtType::Compound, std::move(items)}));
    tag.putLong("DuplicationCooldown", state.duplicationCooldown);
    NbtCompound brain;
    if (const NbtCompound* old = tag.getCompound("Brain"); old != nullptr) {
        brain = *old;
    }
    NbtCompound memories;
    if (state.likedPlayer.has_value()) {
        NbtCompound slot;
        slot.set("value", uuidTag(*state.likedPlayer));
        memories.set(kLikedPlayer, NbtTag::ofCompound(std::move(slot)));
    }
    if (state.likedNoteblock.has_value()) {
        NbtCompound value;
        value.putIntArray("pos", {(*state.likedNoteblock)[0], (*state.likedNoteblock)[1], (*state.likedNoteblock)[2]});
        value.putString("dimension",
                        state.noteblockDimension.empty() ? std::string{kDefaultDimension} : state.noteblockDimension);
        NbtCompound slot;
        slot.set("value", NbtTag::ofCompound(std::move(value)));
        memories.set(kLikedNoteblock, NbtTag::ofCompound(std::move(slot)));
    }
    if (state.noteblockCooldown.has_value()) {
        NbtCompound slot;
        slot.putInt("value", *state.noteblockCooldown);
        memories.set(kNoteblockCooldownMemory, NbtTag::ofCompound(std::move(slot)));
    }
    brain.set("memories", NbtTag::ofCompound(std::move(memories)));
    tag.set("Brain", NbtTag::ofCompound(std::move(brain)));
}

bool considersEqual(const ItemStack& held, const ItemStack& other) {
    if (held.item != other.item) {
        return false;
    }
    const NbtTag* a = held.components.find("minecraft:potion_contents");
    const NbtTag* b = other.components.find("minecraft:potion_contents");
    return (a == nullptr && b == nullptr) || (a != nullptr && b != nullptr && *a == *b);
}

std::int32_t roomFor(const AllayState& state, const ItemStack& stack) {
    if (!state.inventory.has_value() || state.inventory->count <= 0) {
        return std::min(stack.count, maxStackOf(stack));
    }
    if (state.inventory->item != stack.item || !(state.inventory->components == stack.components)) {
        return 0;
    }
    return std::clamp(maxStackOf(*state.inventory) - state.inventory->count, 0, stack.count);
}

} // namespace basalt::server::runtime::entity::allays
