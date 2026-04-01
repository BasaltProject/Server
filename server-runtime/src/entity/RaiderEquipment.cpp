#include <basalt/server/runtime/entity/RaiderEquipment.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <array>
#include <expected>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::entity::raiderEquipment {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

/// parity: `Raid.getBannerComponentPatch`'s layers, bottom first.
constexpr std::array<std::pair<std::string_view, std::string_view>, 8> kPatterns{{{"rhombus", "cyan"},
                                                                                  {"stripe_bottom", "light_gray"},
                                                                                  {"stripe_center", "gray"},
                                                                                  {"border", "light_gray"},
                                                                                  {"stripe_middle", "black"},
                                                                                  {"half_horizontal", "light_gray"},
                                                                                  {"circle", "light_gray"},
                                                                                  {"border", "black"}}};

constexpr float kGuaranteedDrop = 2.0F;

[[nodiscard]] NbtCompound compoundAt(const NbtCompound& root, std::string_view key) {
    const NbtCompound* found = root.getCompound(key);
    return found == nullptr ? NbtCompound{} : *found;
}

void writeSlot(ServerEntity& raider, std::string_view slot, NbtCompound stack) {
    if (!raider.sourceNbt.has_value()) {
        raider.sourceNbt.emplace();
    }
    NbtCompound equipment = compoundAt(*raider.sourceNbt, "equipment");
    equipment.set(slot, NbtTag::ofCompound(std::move(stack)));
    raider.sourceNbt->set("equipment", NbtTag::ofCompound(std::move(equipment)));
}

} // namespace

const NbtCompound& ominousBannerComponents() {
    static const NbtCompound kComponents = [] {
        NbtCompound components;
        std::vector<NbtTag> layers;
        for (const auto& [pattern, colour] : kPatterns) {
            NbtCompound layer;
            layer.putString("pattern", "minecraft:" + std::string{pattern});
            layer.putString("color", std::string{colour});
            layers.push_back(NbtTag::ofCompound(std::move(layer)));
        }
        components.set("minecraft:banner_patterns", NbtTag::ofList(NbtList{NbtType::Compound, std::move(layers)}));
        NbtCompound tooltip;
        tooltip.set("hidden_components",
                    NbtTag::ofList(NbtList{NbtType::String, {NbtTag::ofString("minecraft:banner_patterns")}}));
        components.set("minecraft:tooltip_display", NbtTag::ofCompound(std::move(tooltip)));
        NbtCompound name;
        name.putString("translate", "block.minecraft.ominous_banner");
        components.set("minecraft:item_name", NbtTag::ofCompound(std::move(name)));
        components.putString("minecraft:rarity", "uncommon");
        return components;
    }();
    return kComponents;
}

bool isOminousBanner(const wire::common::Identifier& item, std::int32_t count, const NbtCompound& components) {
    return item == wire::common::Identifier::vanilla("white_banner") && count == 1 &&
           components == ominousBannerComponents();
}

void wearOminousBanner(ServerEntity& raider) {
    NbtCompound banner;
    banner.putString("id", "minecraft:white_banner");
    banner.putInt("count", 1);
    banner.set("components", NbtTag::ofCompound(ominousBannerComponents()));
    writeSlot(raider, "head", std::move(banner));
    NbtCompound chances = compoundAt(*raider.sourceNbt, "drop_chances");
    chances.putFloat("head", kGuaranteedDrop);
    raider.sourceNbt->set("drop_chances", NbtTag::ofCompound(std::move(chances)));
}

bool wearsOminousBanner(const ServerEntity& raider) {
    if (!raider.sourceNbt.has_value()) {
        return false;
    }
    const NbtCompound* equipment = raider.sourceNbt->getCompound("equipment");
    const NbtCompound* head = equipment == nullptr ? nullptr : equipment->getCompound("head");
    if (head == nullptr) {
        return false;
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
        wire::common::Identifier::parse(head->getString("id"));
    const NbtCompound* components = head->getCompound("components");
    return id.has_value() &&
           isOminousBanner(*id, head->getInt("count", 1), components == nullptr ? NbtCompound{} : *components);
}

bool canBeLeader(std::string_view type) { return type != "witch" && type != "ravager"; }

void holdInMainHand(ServerEntity& raider, std::string_view item, std::optional<std::string_view> enchantment,
                    std::int32_t level) {
    NbtCompound stack;
    stack.putString("id", "minecraft:" + std::string{item});
    stack.putInt("count", 1);
    if (enchantment.has_value()) {
        NbtCompound levels;
        levels.putInt("minecraft:" + std::string{*enchantment}, level);
        NbtCompound components;
        components.set("minecraft:enchantments", NbtTag::ofCompound(std::move(levels)));
        stack.set("components", NbtTag::ofCompound(std::move(components)));
    }
    writeSlot(raider, "mainhand", std::move(stack));
}

} // namespace basalt::server::runtime::entity::raiderEquipment
