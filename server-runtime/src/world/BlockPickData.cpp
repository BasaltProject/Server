#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/BlockPickData.hpp>
#include <basalt/server/world/ChunkTypes.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world::blockPickData {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

/// parity: the `BaseContainerBlockEntity` kinds.
constexpr std::array<std::string_view, 12> kBaseContainers{
    "chest",  "trapped_chest", "barrel",  "furnace",     "blast_furnace", "smoker",
    "hopper", "dispenser",     "dropper", "shulker_box", "brewing_stand", "crafter"};

/// parity: the `RandomizableContainerBlockEntity` kinds.
constexpr std::array<std::string_view, 8> kRandomizable{"chest",     "trapped_chest", "barrel",      "hopper",
                                                        "dispenser", "dropper",       "shulker_box", "crafter"};

/// The containers that keep only their items. parity: `CampfireBlockEntity`.
constexpr std::array<std::string_view, 3> kItemsOnly{"campfire", "chiseled_bookshelf", "shelf"};

template <std::size_t N>
[[nodiscard]] bool isOneOf(const std::array<std::string_view, N>& kinds, std::string_view kind) {
    return std::ranges::find(kinds, kind) != kinds.end();
}

void carry(NbtCompound& rest, std::string_view key, std::string_view component, NbtCompound& components) {
    if (const NbtTag* value = rest.find(key); value != nullptr) {
        components.set(component, *value);
        static_cast<void>(rest.remove(key));
    }
}

/// parity: `ItemContainerContents.fromItems`: each stack with the slot it sat in.
[[nodiscard]] NbtTag containerOf(const NbtList* items) {
    NbtList contents{wire::nbt::NbtType::Compound};
    if (items != nullptr) {
        for (const NbtTag& entry : *items) {
            const NbtCompound* stored = entry.get<NbtCompound>();
            if (stored == nullptr || stored->getString("id").empty()) {
                continue;
            }
            NbtCompound item = *stored;
            static_cast<void>(item.remove("Slot"));
            NbtCompound slot;
            slot.putInt("slot", stored->getByte("Slot"));
            slot.set("item", NbtTag::ofCompound(std::move(item)));
            contents.push(NbtTag::ofCompound(std::move(slot)));
        }
    }
    return NbtTag::ofList(std::move(contents));
}

} // namespace

void addBlockData(const server::world::ChunkBlockEntity& entity, NbtCompound& components) {
    const std::string_view kind = entity.type().path();
    // parity: `saveCustomOnly` leaves out the id.
    NbtCompound rest = entity.data();
    for (const std::string_view key : {"id", "x", "y", "z", "keepPacked", "components"}) {
        static_cast<void>(rest.remove(key));
    }
    if (const NbtCompound* own = entity.data().getCompound("components"); own != nullptr) {
        for (const std::string& name : own->names()) {
            components.set(name, *own->find(name));
        }
    }
    if (isOneOf(kBaseContainers, kind)) {
        carry(rest, "CustomName", "minecraft:custom_name", components);
        carry(rest, "lock", "minecraft:lock", components);
        components.set("minecraft:container", containerOf(rest.getList("Items")));
        static_cast<void>(rest.remove("Items"));
        if (isOneOf(kRandomizable, kind) && rest.find("LootTable") != nullptr) {
            NbtCompound loot;
            loot.putString("loot_table", std::string{rest.getString("LootTable")});
            if (const std::int64_t seed = rest.getLong("LootTableSeed"); seed != 0) {
                loot.putLong("seed", seed);
            }
            components.set("minecraft:container_loot", NbtTag::ofCompound(std::move(loot)));
        }
        static_cast<void>(rest.remove("LootTable"));
        static_cast<void>(rest.remove("LootTableSeed"));
    } else if (isOneOf(kItemsOnly, kind)) {
        components.set("minecraft:container", containerOf(rest.getList("Items")));
        static_cast<void>(rest.remove("Items"));
    } else if (kind == "decorated_pot") {
        carry(rest, "sherds", "minecraft:pot_decorations", components);
        NbtList single{wire::nbt::NbtType::Compound};
        if (const NbtCompound* item = rest.getCompound("item"); item != nullptr && !item->getString("id").empty()) {
            NbtCompound slot;
            slot.putInt("slot", 0);
            slot.set("item", NbtTag::ofCompound(*item));
            single.push(NbtTag::ofCompound(std::move(slot)));
        }
        components.set("minecraft:container", NbtTag::ofList(std::move(single)));
        static_cast<void>(rest.remove("item"));
    } else if (kind == "banner") {
        carry(rest, "patterns", "minecraft:banner_patterns", components);
        carry(rest, "CustomName", "minecraft:custom_name", components);
    } else if (kind == "beehive") {
        carry(rest, "bees", "minecraft:bees", components);
    } else if (kind == "beacon") {
        carry(rest, "CustomName", "minecraft:custom_name", components);
        carry(rest, "lock", "minecraft:lock", components);
    } else if (kind == "enchanting_table") {
        carry(rest, "CustomName", "minecraft:custom_name", components);
    } else if (kind == "command_block") {
        carry(rest, "CustomName", "minecraft:custom_name", components);
        static_cast<void>(rest.remove("conditionMet"));
        static_cast<void>(rest.remove("powered"));
    } else if (kind == "sign" || kind == "hanging_sign") {
        carry(rest, "front_text", "minecraft:sign_text_front", components);
        carry(rest, "back_text", "minecraft:sign_text_back", components);
        if (rest.getBoolean("is_waxed")) {
            components.set("minecraft:waxed", NbtTag::ofCompound(NbtCompound{}));
        }
        static_cast<void>(rest.remove("is_waxed"));
    } else if (kind == "skull") {
        // parity: `SkullBlockEntity` collects its three but never takes them off the tag.
        for (const auto& [key, component] :
             {std::pair{"profile", "minecraft:profile"}, std::pair{"note_block_sound", "minecraft:note_block_sound"},
              std::pair{"custom_name", "minecraft:custom_name"}}) {
            if (const NbtTag* value = rest.find(key); value != nullptr) {
                components.set(component, *value);
            }
        }
    }
    // parity: `BlockItem.setBlockEntityData`, nothing when nothing is left.
    if (rest.empty()) {
        static_cast<void>(components.remove("minecraft:block_entity_data"));
        return;
    }
    NbtCompound data;
    data.putString("id", entity.type().toString());
    for (const std::string& name : rest.names()) {
        data.set(name, *rest.find(name));
    }
    components.set("minecraft:block_entity_data", NbtTag::ofCompound(std::move(data)));
}

void addCloneComponents(std::int32_t stateId, const server::world::ChunkBlockEntity* entity, bool includeData,
                        NbtCompound& components) {
    const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return;
    }
    const std::string_view path = block->path();
    const auto state = [stateId](std::string_view property) {
        const std::map<std::string, std::string> properties =
            server::registry::blockStateDefinitions::propertiesOf(stateId);
        const auto found = properties.find(std::string{property});
        return found == properties.end() ? std::string{} : found->second;
    };
    const auto blockState = [&components](std::string_view property, const std::string& value) {
        NbtCompound properties;
        if (const NbtCompound* existing = components.getCompound("minecraft:block_state"); existing != nullptr) {
            properties = *existing;
        }
        properties.putString(property, value);
        components.set("minecraft:block_state", NbtTag::ofCompound(std::move(properties)));
    };
    // parity: `LightBlock.setLightOnStack` and `TestBlock.setModeOnStack`, always.
    if (path == "light") {
        blockState("level", state("level"));
    } else if (path == "test_block") {
        blockState("mode", state("mode"));
    } else if (includeData && (path == "beehive" || path == "bee_nest")) {
        // parity: `BeehiveBlock.getCloneItemStack`, the honey only when the data is asked for.
        blockState("honey_level", state("honey_level"));
    }
    if (entity == nullptr) {
        return;
    }
    // parity: `BannerBlockEntity.getItem` and `DecoratedPotBlockEntity.createDecoratedPotInstance`.
    if (entity->type().path() == "banner") {
        if (const NbtTag* patterns = entity->data().find("patterns"); patterns != nullptr) {
            components.set("minecraft:banner_patterns", *patterns);
        }
        if (const NbtTag* name = entity->data().find("CustomName"); name != nullptr) {
            components.set("minecraft:custom_name", *name);
        }
    } else if (entity->type().path() == "decorated_pot") {
        if (const NbtTag* sherds = entity->data().find("sherds"); sherds != nullptr) {
            components.set("minecraft:pot_decorations", *sherds);
        }
    }
}

} // namespace basalt::server::runtime::world::blockPickData
