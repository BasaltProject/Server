#include "FixSupport.hpp"

#include <array>
#include <map>
#include <string>

namespace basalt::server::datafix {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr float kDefaultDropChance = 0.085F;

// parity: `BoatSplitFix`.
void splitBoat(NbtCompound& entity, const Context&) {
    static const std::map<std::string_view, std::string_view> kBoats{
        {"spruce", "spruce_boat"},     {"birch", "birch_boat"},   {"jungle", "jungle_boat"},
        {"acacia", "acacia_boat"},     {"cherry", "cherry_boat"}, {"dark_oak", "dark_oak_boat"},
        {"mangrove", "mangrove_boat"}, {"bamboo", "bamboo_raft"}};
    static const std::map<std::string_view, std::string_view> kChestBoats{
        {"spruce", "spruce_chest_boat"},     {"birch", "birch_chest_boat"},   {"jungle", "jungle_chest_boat"},
        {"acacia", "acacia_chest_boat"},     {"cherry", "cherry_chest_boat"}, {"dark_oak", "dark_oak_chest_boat"},
        {"mangrove", "mangrove_chest_boat"}, {"bamboo", "bamboo_chest_raft"}};
    const std::string id = namespaced(entity.getString("id"));
    const bool chest = id == "minecraft:chest_boat";
    if (id != "minecraft:boat" && !chest) {
        return;
    }
    const auto& names = chest ? kChestBoats : kBoats;
    const std::string_view variant = entity.getString("Type");
    const auto found = names.find(variant);
    entity.putString("id", "minecraft:" + std::string{found != names.end() ? found->second
                                                                           : (chest ? "oak_chest_boat" : "oak_boat")});
    entity.remove("Type");
}

// parity: `EntitySalmonSizeFix`.
void salmonSize(NbtCompound& entity, const Context&) {
    const std::string_view type = entity.contains("type") ? entity.getString("type") : "medium";
    if (type != "large") {
        entity.putString("type", "medium");
    }
}

// parity: `EntityFieldsRenameFix` for the TNT minecart fuse.
void tntMinecartFuse(NbtCompound& entity, const Context&) { rename(entity, "TNTFuse", "fuse"); }

// parity: `DropChancesFormatFix`.
void dropChances(NbtCompound& entity, const Context&) {
    const auto listOf = [&entity](std::string_view key) {
        std::vector<float> chances;
        if (const NbtTag* held = entity.find(key); held != nullptr && held->get<NbtList>() != nullptr) {
            for (const NbtTag& value : *held->get<NbtList>()) {
                const float* f = value.get<float>();
                chances.push_back(f != nullptr ? *f : kDefaultDropChance);
            }
        }
        return chances;
    };
    const std::vector<float> armor = listOf("ArmorDropChances");
    const std::vector<float> hands = listOf("HandDropChances");
    float body = kDefaultDropChance;
    if (const NbtTag* held = entity.find("body_armor_drop_chance"); held != nullptr) {
        if (const float* f = held->get<float>(); f != nullptr) {
            body = *f;
        } else if (const double* d = held->get<double>(); d != nullptr) {
            body = static_cast<float>(*d);
        }
    }
    entity.remove("ArmorDropChances");
    entity.remove("HandDropChances");
    entity.remove("body_armor_drop_chance");
    NbtCompound slots;
    static constexpr std::array<std::string_view, 4> kArmor{"feet", "legs", "chest", "head"};
    static constexpr std::array<std::string_view, 2> kHands{"mainhand", "offhand"};
    for (std::size_t i = 0; i < kArmor.size() && i < armor.size(); ++i) {
        if (armor[i] != kDefaultDropChance) {
            slots.putFloat(kArmor[i], armor[i]);
        }
    }
    for (std::size_t i = 0; i < kHands.size() && i < hands.size(); ++i) {
        if (hands[i] != kDefaultDropChance) {
            slots.putFloat(kHands[i], hands[i]);
        }
    }
    if (body != kDefaultDropChance) {
        slots.putFloat("body", body);
    }
    if (!slots.empty()) {
        entity.set("drop_chances", NbtTag::ofCompound(std::move(slots)));
    }
}

// parity: `AreaEffectCloudDurationScaleFix`.
void areaEffectCloudScale(NbtCompound& entity, const Context&) { entity.putFloat("potion_duration_scale", 0.25F); }

void saddleDropChance(NbtCompound& entity) {
    NbtCompound chances;
    if (const NbtTag* held = entity.find("drop_chances"); held != nullptr && held->get<NbtCompound>() != nullptr) {
        chances = *held->get<NbtCompound>();
    }
    chances.putFloat("saddle", 2.0F);
    entity.set("drop_chances", NbtTag::ofCompound(std::move(chances)));
}

// parity: `SaddleEquipmentSlotFix`.
void saddleSlot(NbtCompound& entity, const Context&) {
    static constexpr std::array<std::string_view, 8> kWithItem{
        "minecraft:horse", "minecraft:skeleton_horse", "minecraft:zombie_horse", "minecraft:donkey",
        "minecraft:mule",  "minecraft:camel",          "minecraft:llama",        "minecraft:trader_llama"};
    const std::string id = namespaced(entity.getString("id"));
    if (std::ranges::find(kWithItem, id) != kWithItem.end()) {
        if (entity.contains("SaddleItem")) {
            rename(entity, "SaddleItem", "saddle");
            saddleDropChance(entity);
        }
    } else if (id == "minecraft:pig" || id == "minecraft:strider") {
        const bool saddled = entity.getBoolean("Saddle");
        entity.remove("Saddle");
        if (saddled) {
            NbtCompound saddle;
            saddle.putString("id", "minecraft:saddle");
            saddle.putInt("count", 1);
            entity.set("saddle", NbtTag::ofCompound(std::move(saddle)));
            saddleDropChance(entity);
        }
    }
}

// parity: `EquipmentFormatFix`, where an empty compound is the placeholder for no item.
void equipmentFormat(NbtCompound& entity, const Context&) {
    const auto itemAt = [&entity](std::string_view key, std::size_t index) -> std::optional<NbtTag> {
        const NbtTag* held = entity.find(key);
        const NbtList* list = held == nullptr ? nullptr : held->get<NbtList>();
        if (list == nullptr || index >= list->size()) {
            return std::nullopt;
        }
        const NbtCompound* item = (*list)[index].get<NbtCompound>();
        return item == nullptr || !item->contains("id") ? std::nullopt : std::optional{(*list)[index]};
    };
    const auto single = [&entity](std::string_view key) -> std::optional<NbtTag> {
        const NbtTag* held = entity.find(key);
        const NbtCompound* item = held == nullptr ? nullptr : held->get<NbtCompound>();
        return item == nullptr || !item->contains("id") ? std::nullopt : std::optional{*held};
    };
    NbtCompound equipment;
    const auto put = [&equipment](std::string_view slot, const std::optional<NbtTag>& item) {
        if (item.has_value()) {
            equipment.set(slot, *item);
        }
    };
    put("mainhand", itemAt("HandItems", 0));
    put("offhand", itemAt("HandItems", 1));
    put("feet", itemAt("ArmorItems", 0));
    put("legs", itemAt("ArmorItems", 1));
    put("chest", itemAt("ArmorItems", 2));
    put("head", itemAt("ArmorItems", 3));
    put("body", single("body_armor_item"));
    put("saddle", single("saddle"));
    for (const std::string_view old : {"ArmorItems", "HandItems", "body_armor_item", "saddle"}) {
        entity.remove(old);
    }
    if (!equipment.empty()) {
        entity.set("equipment", NbtTag::ofCompound(std::move(equipment)));
    }
}

// parity: `EntityFallDistanceFloatToDoubleFix`.
void fallDistanceToDouble(NbtCompound& entity, const Context&) {
    const NbtTag* held = entity.find("FallDistance");
    if (held == nullptr) {
        return;
    }
    double value = 0.0;
    if (const float* f = held->get<float>(); f != nullptr) {
        value = static_cast<double>(*f);
    } else if (const double* d = held->get<double>(); d != nullptr) {
        value = *d;
    }
    entity.remove("FallDistance");
    entity.putDouble("fall_distance", value);
}

// parity: `ThrownPotionSplitFix`.
void splitThrownPotion(NbtCompound& entity, const Context&) {
    if (namespaced(entity.getString("id")) != "minecraft:potion") {
        return;
    }
    const NbtCompound* item = entity.getCompound("Item");
    const std::string itemId = item == nullptr ? std::string{} : namespaced(item->getString("id"));
    entity.putString("id",
                     itemId == "minecraft:lingering_potion" ? "minecraft:lingering_potion" : "minecraft:splash_potion");
}

// parity: `CopperGolemWeatherStateFix`.
void copperGolemWeatherState(NbtCompound& entity, const Context&) {
    const NbtTag* held = entity.find("weather_state");
    if (held == nullptr) {
        return;
    }
    std::int32_t state = 0;
    if (const auto* i = held->get<std::int32_t>(); i != nullptr) {
        state = *i;
    } else if (const auto* b = held->get<std::int8_t>(); b != nullptr) {
        state = *b;
    }
    static constexpr std::array<std::string_view, 4> kNames{"unaffected", "exposed", "weathered", "oxidized"};
    entity.putString("weather_state",
                     std::string{kNames[static_cast<std::size_t>(state >= 0 && state < 4 ? state : 0)]});
}

} // namespace

void addEntityFixes(std::vector<Fix>& fixes) {
    fixes.insert(fixes.end(), {
                                  {4067, Target::Entity, {}, splitBoat},
                                  {4081, Target::Entity, "minecraft:salmon", salmonSize},
                                  {4173, Target::Entity, "minecraft:tnt_minecart", tntMinecartFuse},
                                  {4293, Target::Entity, {}, dropChances},
                                  {4296, Target::Entity, "minecraft:area_effect_cloud", areaEffectCloudScale},
                                  {4300, Target::Entity, {}, saddleSlot},
                                  {4301, Target::Entity, {}, equipmentFormat},
                                  {4303, Target::Entity, {}, fallDistanceToDouble},
                                  {4306, Target::Entity, {}, splitThrownPotion},
                                  {4535, Target::Entity, "minecraft:copper_golem", copperGolemWeatherState},
                              });
}

} // namespace basalt::server::datafix
