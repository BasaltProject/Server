#include "ItemSlots.hpp"

#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>

namespace basalt::server::runtime::command::commands::itemSlots {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;
using wire::text::Component;

constexpr std::int32_t kArmourStart = 36;
constexpr std::int32_t kOffhand = 40;
constexpr std::int32_t kHotbarSize = 9;
constexpr std::int32_t kMainSize = 27;

/// parity: `SlotRanges`' own `container.0` to `container.53`, a double chest's worth.
constexpr std::int32_t kContainerSlots = 54;

/// The number after a `hotbar.`/`inventory.` prefix, when the whole tail is one and it is in range.
[[nodiscard]] std::optional<std::int32_t> indexIn(std::string_view tail, std::int32_t count) {
    std::int32_t value = 0;
    const char* first = tail.data();
    const std::from_chars_result result = std::from_chars(first, first + tail.size(), value);
    if (result.ec != std::errc{} || result.ptr != first + tail.size() || value < 0 || value >= count) {
        return std::nullopt;
    }
    return value;
}

/// A container's `Items`, as the compounds it really holds.
[[nodiscard]] std::vector<NbtCompound> itemsOf(const NbtCompound& data) {
    std::vector<NbtCompound> entries;
    const NbtTag* items = data.find("Items");
    const auto* list = items == nullptr ? nullptr : items->get<wire::nbt::NbtList>();
    if (list == nullptr) {
        return entries;
    }
    for (const NbtTag& entry : list->elements()) {
        if (const auto* compound = entry.get<NbtCompound>(); compound != nullptr) {
            entries.push_back(*compound);
        }
    }
    return entries;
}

/// parity: `ItemStackWithSlot`'s codec reads `Slot` as an unsigned byte, and that is what every
/// other reader in this server expects too: writing it as an int leaves a container that `/item`
/// filled looking empty to the dispenser that owns it.
[[nodiscard]] std::int32_t slotIn(const NbtCompound& entry) {
    return entry.contains("Slot") ? entry.getByte("Slot", -1) : -1;
}

} // namespace

std::optional<std::int32_t> resolve(const std::string& name, const bool container) {
    if (name.starts_with("container.")) {
        const std::optional<std::int32_t> index = indexIn(std::string_view{name}.substr(10), kContainerSlots);
        if (!index.has_value()) {
            return std::nullopt;
        }
        if (container) {
            return *index;
        }
        // parity: on a player `container.N` is `Inventory` slot N, whose hotbar is the first nine.
        if (*index < kHotbarSize) {
            return player::PlayerInventory::kHotbarStart + *index;
        }
        if (*index < kArmourStart) {
            return *index - kHotbarSize;
        }
        return *index > kOffhand ? std::nullopt : std::optional{*index};
    }
    if (name.starts_with("hotbar.")) {
        const std::optional<std::int32_t> index = indexIn(std::string_view{name}.substr(7), kHotbarSize);
        return index.has_value() ? std::optional{player::PlayerInventory::kHotbarStart + *index} : std::nullopt;
    }
    if (name.starts_with("inventory.")) {
        return indexIn(std::string_view{name}.substr(10), kMainSize);
    }
    if (name == "armor.feet") {
        return kArmourStart;
    }
    if (name == "armor.legs") {
        return kArmourStart + 1;
    }
    if (name == "armor.chest") {
        return kArmourStart + 2;
    }
    if (name == "armor.head") {
        return kArmourStart + 3;
    }
    if (name == "weapon.offhand") {
        return kOffhand;
    }
    // `weapon` and `weapon.mainhand` mean "whatever is held", which moves with the held slot rather
    // than being a slot of its own, so they are refused here instead of resolving to the wrong one.
    return std::nullopt;
}

const std::vector<std::string>& names() {
    static const std::vector<std::string> all = [] {
        std::vector<std::string> built;
        for (std::int32_t i = 0; i < kHotbarSize; i++) {
            built.push_back("hotbar." + std::to_string(i));
        }
        for (std::int32_t i = 0; i < kMainSize; i++) {
            built.push_back("inventory." + std::to_string(i));
        }
        for (std::int32_t i = 0; i < kContainerSlots; i++) {
            built.push_back("container." + std::to_string(i));
        }
        built.insert(built.end(), {"armor.feet", "armor.legs", "armor.chest", "armor.head", "weapon.offhand"});
        return built;
    }();
    return all;
}

Parsed<std::int32_t> slotOf(const CommandContext& context, std::string_view name, const bool container) {
    const auto* named = context.get<std::string>(std::string{name});
    const std::optional<std::int32_t> slot = named == nullptr ? std::nullopt : resolve(*named, container);
    if (!slot.has_value()) {
        return std::unexpected{CommandSyntaxError{
            .component =
                Component::translatable("slot.unknown", {Component::text(named == nullptr ? std::string{} : *named)})}};
    }
    return *slot;
}

std::vector<std::pair<std::int32_t, storage::StoredItemStack>> containerItems(const CommandContext& context,
                                                                              const std::array<std::int32_t, 3>& at) {
    std::vector<std::pair<std::int32_t, storage::StoredItemStack>> held;
    const Parsed<world::LoadedWorld*> world = context.world();
    if (!world.has_value()) {
        return held;
    }
    const server::world::Chunk* chunk = (*world)->state().chunkIfLoaded(at[0] >> 4, at[2] >> 4);
    if (chunk == nullptr) {
        return held;
    }
    const server::world::ChunkBlockEntity* existing = chunk->blockEntityAt(at[0] & 15, at[1], at[2] & 15);
    if (existing == nullptr) {
        return held;
    }
    for (const NbtCompound& entry : itemsOf(existing->data())) {
        const NbtTag* id = entry.find("id");
        const NbtTag* count = entry.find("count");
        const auto* name = id == nullptr ? nullptr : id->get<std::string>();
        const auto* amount = count == nullptr ? nullptr : count->get<std::int32_t>();
        if (name != nullptr) {
            held.emplace_back(slotIn(entry),
                              storage::StoredItemStack{.id = *name, .count = amount == nullptr ? 1 : *amount});
        }
    }
    return held;
}

std::optional<CommandSyntaxError>
writeContainer(const CommandContext& context, const std::array<std::int32_t, 3>& at,
               const std::vector<std::pair<std::int32_t, storage::StoredItemStack>>& written,
               world::RealRecipes* recipes) {
    const Parsed<world::LoadedWorld*> world = context.world();
    if (!world.has_value()) {
        return world.error();
    }
    server::world::Chunk* chunk = (*world)->state().chunkIfLoaded(at[0] >> 4, at[2] >> 4);
    if (chunk == nullptr) {
        return CommandSyntaxError{.component = Component::translatable("argument.pos.unloaded")};
    }
    const server::world::ChunkBlockEntity* existing = chunk->blockEntityAt(at[0] & 15, at[1], at[2] & 15);
    if (existing == nullptr) {
        return CommandSyntaxError{
            .component = Component::translatable(
                "commands.item.target.not_a_container",
                {Component::text(std::to_string(at[0]) + " " + std::to_string(at[1]) + " " + std::to_string(at[2]))})};
    }
    std::vector<NbtTag> entries;
    for (const NbtCompound& entry : itemsOf(existing->data())) {
        const std::int32_t slot = slotIn(entry);
        const bool replaced = std::ranges::any_of(written, [slot](const auto& one) { return one.first == slot; });
        if (!replaced) {
            entries.push_back(NbtTag::ofCompound(entry));
        }
    }
    for (const auto& [slot, stack] : written) {
        NbtCompound entry;
        entry.putByte("Slot", static_cast<std::int8_t>(slot));
        entry.set("id", NbtTag::ofString(stack.id));
        entry.set("count", NbtTag::ofInt(stack.count));
        // parity: the components the command named travel with the item, exactly as `ItemStack.CODEC`
        // writes them: without this an enchanted sword put in a chest by command came out plain.
        if (stack.components.size() > 0) {
            entry.set("components", NbtTag::ofCompound(stack.components));
        }
        entries.push_back(NbtTag::ofCompound(entry));
    }
    // parity: `ContainerHelper.saveAllItems` walks the slots in order, so the list on disk is in
    // slot order however the slots were filled. Appending what the command wrote left a brewing
    // stand whose four items were listed backwards.
    std::ranges::sort(entries, [](const NbtTag& one, const NbtTag& other) {
        const NbtCompound* first = one.get<NbtCompound>();
        const NbtCompound* second = other.get<NbtCompound>();
        return (first == nullptr ? 0 : slotIn(*first)) < (second == nullptr ? 0 : slotIn(*second));
    });
    NbtCompound data = existing->data();
    if (world::blockInventory::holdsOneItem(existing->type().path())) {
        // parity: a decorated pot keeps its one stack under `item`, so writing a list here would
        // leave a tag vanilla never wrote and lose what the command put in.
        world::blockInventory::Slots one(1);
        for (const auto& [slot, stack] : written) {
            if (slot == 0) {
                const std::expected<wire::common::Identifier, wire::common::IdentifierError> item =
                    wire::common::Identifier::parse(stack.id);
                if (item.has_value()) {
                    one[0] = server::world::loot::ItemStack{
                        .item = *item, .count = stack.count, .components = stack.components};
                }
            }
        }
        world::blockInventory::writeFor(existing->type().path(), data, one);
    } else {
        data.set("Items", NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::Compound, std::move(entries)}));
    }
    // parity: `AbstractFurnaceBlockEntity.setItem`, which sets the cooking time from the ingredient
    // it was handed and puts the progress back to zero. Every write to a furnace's slots goes through
    // it, a command's included, so the invariant it leaves behind is kept here: while there is an
    // input, the total is that input's own cooking time.
    if (const std::optional<server::world::crafting::Cooker> cooker =
            world::furnaces::cookerFor(existing->type().path());
        cooker.has_value() && recipes != nullptr) {
        world::furnaces::State state = world::furnaces::read(data);
        if (state.items[world::furnaces::kInputSlot].has_value()) {
            const std::int32_t wanted =
                world::furnaces::cookTimeFor(state, [recipes, cooker](const server::world::loot::ItemStack& stack) {
                    return recipes->cookingFor(*cooker, stack);
                });
            if (state.cookingTotalTime != wanted) {
                state.cookingTotalTime = wanted;
                state.cookingTimer = 0;
                world::furnaces::write(state, data);
            }
        }
    }
    // Only the containers that say so: a hopper filled by command leaves its comparator none the
    // wiser, and a furnace speaks only for the slot it cooks from.
    const std::vector<NbtCompound> before = itemsOf(existing->data());
    const std::string_view path = existing->type().path();
    const bool tells = std::ranges::any_of(written, [&](const auto& one) {
        const auto was = std::ranges::find_if(before, [&](const NbtCompound& e) { return slotIn(e) == one.first; });
        const bool sameItem = was != before.end() && was->getString("id", std::string{}) == one.second.id;
        return world::containers::setItemTellsComparators(path, one.first, sameItem);
    });
    chunk->setBlockEntity(existing->withData(std::move(data)));
    if (tells) {
        world::redstoneComponents::scheduleComparatorsAround((*world)->state(), (*world)->name(), at[0], at[1], at[2],
                                                             world::comparatorReading::of((*world)->state()));
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::command::commands::itemSlots
