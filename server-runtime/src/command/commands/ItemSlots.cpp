#include "ItemSlots.hpp"

#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <charconv>

namespace basalt::server::runtime::command::commands::itemSlots {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;
using wire::text::Component;

constexpr std::int32_t kArmourStart = 36;
constexpr std::int32_t kOffhand = 40;
constexpr std::int32_t kHotbarSize = 9;
constexpr std::int32_t kMainSize = 27;

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

[[nodiscard]] std::int32_t slotIn(const NbtCompound& entry) {
    const NbtTag* slot = entry.find("Slot");
    const auto* value = slot == nullptr ? nullptr : slot->get<std::int32_t>();
    return value == nullptr ? -1 : *value;
}

} // namespace

std::optional<std::int32_t> resolve(const std::string& name) {
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
        built.insert(built.end(), {"armor.feet", "armor.legs", "armor.chest", "armor.head", "weapon.offhand"});
        return built;
    }();
    return all;
}

Parsed<std::int32_t> slotOf(const CommandContext& context, std::string_view name) {
    const auto* named = context.get<std::string>(std::string{name});
    const std::optional<std::int32_t> slot = named == nullptr ? std::nullopt : resolve(*named);
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
               const std::vector<std::pair<std::int32_t, storage::StoredItemStack>>& written) {
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
        entry.set("Slot", NbtTag::ofInt(slot));
        entry.set("id", NbtTag::ofString(stack.id));
        entry.set("count", NbtTag::ofInt(stack.count));
        entries.push_back(NbtTag::ofCompound(entry));
    }
    NbtCompound data = existing->data();
    data.set("Items", NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::Compound, std::move(entries)}));
    chunk->setBlockEntity(existing->withData(std::move(data)));
    return std::nullopt;
}

} // namespace basalt::server::runtime::command::commands::itemSlots
