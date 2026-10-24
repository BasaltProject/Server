#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/entity/Piglins.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>

#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity::piglins {
namespace {

using server::world::loot::ItemStack;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr std::string_view kAngryAt = "minecraft:angry_at";
constexpr std::string_view kHuntedRecently = "minecraft:hunted_recently";
constexpr std::string_view kUniversalAnger = "minecraft:universal_anger";
constexpr std::string_view kHome = "minecraft:home";

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

[[nodiscard]] bool sameStack(const ItemStack& a, const ItemStack& b) {
    return a.item == b.item && a.components == b.components;
}

void putTimed(NbtCompound& memories, std::string_view name, NbtTag value, std::int64_t until, std::int64_t now) {
    NbtCompound slot;
    slot.set("value", std::move(value));
    if (until >= 0) {
        slot.putLong("ttl", std::max<std::int64_t>(until - now, 0));
    }
    memories.set(name, NbtTag::ofCompound(std::move(slot)));
}

} // namespace

bool isPiglin(const wire::common::Identifier& type) {
    return type == wire::common::Identifier::vanilla("piglin") || isBrute(type);
}

bool isBrute(const wire::common::Identifier& type) { return type == wire::common::Identifier::vanilla("piglin_brute"); }

PiglinState& stateOf(ServerEntity& piglin) {
    if (piglin.piglin != nullptr) {
        return *piglin.piglin;
    }
    piglin.piglin = std::make_shared<PiglinState>();
    PiglinState& state = *piglin.piglin;
    state.brute = isBrute(piglin.type());
    if (!piglin.sourceNbt.has_value()) {
        return state;
    }
    const NbtCompound& tag = *piglin.sourceNbt;
    state.cannotHunt = tag.getBoolean("CannotHunt");
    for (const auto& [key, into] : {std::pair{"Bukkit.BarterList", &state.barterItems},
                                    std::pair{"Bukkit.InterestList", &state.interestItems}}) {
        if (const NbtList* listed = tag.getList(key); listed != nullptr) {
            for (const NbtTag& entry : listed->elements()) {
                if (const std::string* name = entry.get<std::string>(); name != nullptr) {
                    into->insert(*name);
                }
            }
        }
    }
    if (const NbtList* inventory = tag.getList("Inventory"); inventory != nullptr) {
        std::size_t slot = 0;
        for (const NbtTag& entry : inventory->elements()) {
            const NbtCompound* stack = entry.get<NbtCompound>();
            if (stack != nullptr && slot < kInventorySize) {
                state.inventory[slot++] = world::blockInventory::readStack(*stack);
            }
        }
    }
    const NbtCompound* brain = tag.getCompound("Brain");
    state.memoriesSeeded = brain != nullptr;
    const NbtCompound* memories = brain == nullptr ? nullptr : brain->getCompound("memories");
    if (memories == nullptr) {
        return state;
    }
    // Expiries are kept as the ticks left; the first turn turns them into absolute times.
    if (const NbtCompound* angry = memories->getCompound(kAngryAt); angry != nullptr) {
        if (const std::optional<wire::common::Uuid> uuid = uuidIn(*angry, "value"); uuid.has_value()) {
            state.angryAt.value = *uuid;
            state.angryAt.until = angry->contains("ttl") ? -2 - angry->getLong("ttl") : -1;
        }
    }
    if (const NbtCompound* universal = memories->getCompound(kUniversalAnger); universal != nullptr) {
        state.universalAnger.value = universal->getBoolean("value");
        state.universalAnger.until = universal->contains("ttl") ? -2 - universal->getLong("ttl") : -1;
    }
    if (const NbtCompound* hunted = memories->getCompound(kHuntedRecently); hunted != nullptr) {
        state.huntedRecently.value = true;
        state.huntedRecently.until = hunted->contains("ttl") ? -2 - hunted->getLong("ttl") : -1;
    }
    if (const NbtCompound* home = memories->getCompound(kHome); home != nullptr) {
        if (const NbtCompound* value = home->getCompound("value"); value != nullptr) {
            if (const std::span<const std::int32_t> pos = value->getIntArray("pos"); pos.size() == 3) {
                state.home = Pos{pos[0], pos[1], pos[2]};
            }
        }
    }
    return state;
}

void write(const ServerEntity& piglin, NbtCompound& tag) {
    if (piglin.piglin == nullptr) {
        return;
    }
    const PiglinState& state = *piglin.piglin;
    const std::int64_t gameTime = state.lastTick;
    for (const auto& [key, from] : {std::pair{"Bukkit.BarterList", &state.barterItems},
                                    std::pair{"Bukkit.InterestList", &state.interestItems}}) {
        std::vector<NbtTag> names;
        for (const std::string& name : *from) {
            names.push_back(NbtTag::ofString(name));
        }
        if (names.empty()) {
            tag.remove(key);
        } else {
            tag.set(key, NbtTag::ofList(NbtList{NbtType::String, std::move(names)}));
        }
    }
    if (!state.brute) {
        tag.putBoolean("CannotHunt", state.cannotHunt);
        std::vector<NbtTag> items;
        for (const std::optional<ItemStack>& stack : state.inventory) {
            if (stack.has_value() && stack->count > 0) {
                items.push_back(NbtTag::ofCompound(world::blockInventory::writeStack(*stack)));
            }
        }
        tag.set("Inventory", NbtTag::ofList(NbtList{NbtType::Compound, std::move(items)}));
    }
    NbtCompound brain;
    if (const NbtCompound* old = tag.getCompound("Brain"); old != nullptr) {
        brain = *old;
    }
    NbtCompound memories;
    if (state.angryAt.has(gameTime)) {
        putTimed(memories, kAngryAt, uuidTag(*state.angryAt.value), state.angryAt.until, gameTime);
    }
    if (state.universalAnger.has(gameTime)) {
        putTimed(memories, kUniversalAnger, NbtTag::ofBoolean(*state.universalAnger.value), state.universalAnger.until,
                 gameTime);
    }
    if (state.huntedRecently.has(gameTime)) {
        putTimed(memories, kHuntedRecently, NbtTag::ofBoolean(true), state.huntedRecently.until, gameTime);
    }
    if (state.home.has_value()) {
        NbtCompound value;
        value.putIntArray("pos", {(*state.home)[0], (*state.home)[1], (*state.home)[2]});
        value.putString("dimension", "minecraft:the_nether");
        putTimed(memories, kHome, NbtTag::ofCompound(std::move(value)), -1, gameTime);
    }
    brain.set("memories", NbtTag::ofCompound(std::move(memories)));
    tag.set("Brain", NbtTag::ofCompound(std::move(brain)));
}

std::optional<ItemStack> addToInventory(PiglinState& state, ItemStack stack) {
    // parity: `SimpleContainer.addItem`: onto matching stacks first, then into empty slots.
    for (std::optional<ItemStack>& slot : state.inventory) {
        if (stack.count <= 0) {
            break;
        }
        if (slot.has_value() && sameStack(*slot, stack)) {
            const std::int32_t moved = std::min(stack.count, maxStackOf(*slot) - slot->count);
            if (moved > 0) {
                slot->count += moved;
                stack.count -= moved;
            }
        }
    }
    for (std::optional<ItemStack>& slot : state.inventory) {
        if (stack.count <= 0) {
            break;
        }
        if (!slot.has_value() || slot->count <= 0) {
            slot = stack;
            stack.count = 0;
        }
    }
    return stack.count > 0 ? std::optional{stack} : std::nullopt;
}

bool canAddToInventory(const PiglinState& state, const ItemStack& stack) {
    return std::ranges::any_of(state.inventory, [&stack](const std::optional<ItemStack>& slot) {
        return !slot.has_value() || slot->count <= 0 || (sameStack(*slot, stack) && slot->count < maxStackOf(*slot));
    });
}

} // namespace basalt::server::runtime::entity::piglins
