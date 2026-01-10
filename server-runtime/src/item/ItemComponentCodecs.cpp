#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/item/ItemComponentPayloads.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <ranges>
#include <variant>

namespace basalt::server::runtime::item::itemComponentCodecs {
namespace {

namespace reg = server::registry;

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;
using wire::protocol::play::ItemComponent;
namespace dataComponent = wire::protocol::play::dataComponent;

/// `minecraft:enchantments`, where an enchanted stack keeps its levels by name.
constexpr std::string_view kEnchantments = "minecraft:enchantments";
constexpr std::string_view kStoredEnchantments = "minecraft:stored_enchantments";

/// parity: `Rarity`'s own order, which is what its `idMapper` stream codec writes.
constexpr std::array<std::string_view, 4> kRarities{"common", "uncommon", "rare", "epic"};

/// The components whose whole payload is one number.
constexpr std::array<std::pair<std::string_view, std::int32_t>, 4> kNumbers{
    {{"minecraft:max_stack_size", dataComponent::kMaxStackSize},
     {"minecraft:max_damage", dataComponent::kMaxDamage},
     {"minecraft:damage", dataComponent::kDamage},
     {"minecraft:repair_cost", dataComponent::kRepairCost}}};

/// The components that carry nothing but their own presence.
constexpr std::array<std::pair<std::string_view, std::int32_t>, 2> kUnits{
    {{"minecraft:unbreakable", dataComponent::kUnbreakable},
     {"minecraft:creative_slot_lock", dataComponent::kCreativeSlotLock}}};

constexpr std::array<std::pair<std::string_view, std::int32_t>, 2> kTexts{
    {{"minecraft:custom_name", dataComponent::kCustomName}, {"minecraft:item_name", dataComponent::kItemName}}};

[[nodiscard]] std::optional<std::int32_t> idIn(std::span<const std::pair<std::string_view, std::int32_t>> table,
                                               std::string_view name) {
    const auto found = std::ranges::find(table, name, &std::pair<std::string_view, std::int32_t>::first);
    return found == table.end() ? std::nullopt : std::optional{found->second};
}

[[nodiscard]] std::optional<std::int32_t> intOf(const NbtTag& value) {
    if (const std::int32_t* asInt = value.get<std::int32_t>(); asInt != nullptr) {
        return *asInt;
    }
    if (const std::int8_t* asByte = value.get<std::int8_t>(); asByte != nullptr) {
        return static_cast<std::int32_t>(*asByte);
    }
    return std::nullopt;
}

} // namespace

const std::set<std::string, std::less<>>& typed() {
    static const std::set<std::string, std::less<>> names = [] {
        std::set<std::string, std::less<>> built;
        for (const auto& [name, id] : kNumbers) {
            built.emplace(name);
        }
        for (const auto& [name, id] : kUnits) {
            built.emplace(name);
        }
        for (const auto& [name, id] : kTexts) {
            built.emplace(name);
        }
        built.emplace(kEnchantments);
        built.emplace(kStoredEnchantments);
        built.emplace("minecraft:custom_data");
        built.emplace("minecraft:lore");
        built.emplace("minecraft:rarity");
        built.emplace("minecraft:dyed_color");
        built.emplace("minecraft:enchantment_glint_override");
        return built;
    }();
    return names;
}

bool exists(std::string_view name) {
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(name);
    return parsed.has_value() && reg::dataComponents::idOf(*parsed).has_value();
}

bool isSent(std::string_view name) {
    return typed().contains(name) || itemComponentPayloads::supported().contains(name);
}

std::vector<std::string> notDrawn(const NbtCompound& components) {
    std::vector<std::string> missing;
    for (const std::string& name : components.names()) {
        if (!isSent(name)) {
            missing.push_back(name);
        }
    }
    return missing;
}

std::optional<ItemComponent> toWire(std::string_view name, const NbtTag& value, const EnchantmentId& enchantmentId) {
    if (const std::optional<std::int32_t> id = idIn(kNumbers, name); id.has_value()) {
        const std::optional<std::int32_t> number = intOf(value);
        return number.has_value() ? std::optional{ItemComponent{*id, ItemComponent::Number{.value = *number}}}
                                  : std::nullopt;
    }
    if (const std::optional<std::int32_t> id = idIn(kUnits, name); id.has_value()) {
        return ItemComponent{*id, ItemComponent::Unit{}};
    }
    if (const std::optional<std::int32_t> id = idIn(kTexts, name); id.has_value()) {
        return ItemComponent{*id, ItemComponent::Text{.text = wire::text::componentNbt::deserialize(value)}};
    }
    if (name == kEnchantments || name == kStoredEnchantments) {
        const NbtCompound* compound = value.get<NbtCompound>();
        if (compound == nullptr) {
            return std::nullopt;
        }
        std::vector<std::pair<std::int32_t, std::int32_t>> levels;
        for (const std::string& key : compound->names()) {
            const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(key);
            if (!parsed.has_value()) {
                continue;
            }
            const std::optional<std::int32_t> id = enchantmentId(*parsed);
            if (!id.has_value()) {
                continue;
            }
            levels.emplace_back(*id, compound->getInt(key));
        }
        if (levels.empty()) {
            return std::nullopt;
        }
        const bool stored = name == kStoredEnchantments;
        return ItemComponent{stored ? dataComponent::kStoredEnchantments : dataComponent::kEnchantments,
                             ItemComponent::Enchantments{.levels = std::move(levels), .stored = stored}};
    }
    if (name == "minecraft:custom_data") {
        const NbtCompound* compound = value.get<NbtCompound>();
        return compound == nullptr
                   ? std::nullopt
                   : std::optional{ItemComponent{dataComponent::kCustomData, ItemComponent::CustomData{*compound}}};
    }
    if (name == "minecraft:lore") {
        const NbtList* list = value.get<NbtList>();
        if (list == nullptr) {
            return std::nullopt;
        }
        std::vector<wire::text::Component> lines;
        lines.reserve(list->size());
        for (const NbtTag& line : *list) {
            lines.push_back(wire::text::componentNbt::deserialize(line));
        }
        return ItemComponent{dataComponent::kLore, ItemComponent::Lore{.lines = std::move(lines)}};
    }
    if (name == "minecraft:rarity") {
        const std::string* text = value.get<std::string>();
        if (text == nullptr) {
            return std::nullopt;
        }
        const auto found = std::ranges::find(kRarities, *text);
        return found == kRarities.end()
                   ? std::nullopt
                   : std::optional{ItemComponent{
                         dataComponent::kRarity,
                         ItemComponent::Number{.value = static_cast<std::int32_t>(found - kRarities.begin())}}};
    }
    if (name == "minecraft:dyed_color") {
        const std::optional<std::int32_t> rgb = intOf(value);
        return rgb.has_value()
                   ? std::optional{ItemComponent{dataComponent::kDyedColor, ItemComponent::Color{.rgb = *rgb}}}
                   : std::nullopt;
    }
    if (name == "minecraft:enchantment_glint_override") {
        const std::optional<std::int32_t> flag = intOf(value);
        return flag.has_value() ? std::optional{ItemComponent{dataComponent::kEnchantmentGlintOverride,
                                                              ItemComponent::Flag{.value = *flag != 0}}}
                                : std::nullopt;
    }
    // Everything else with a network form of its own is written straight into bytes.
    return itemComponentPayloads::encode(name, value);
}

std::vector<ItemComponent> toWire(const NbtCompound& components, const EnchantmentId& enchantmentId) {
    std::vector<ItemComponent> wire;
    for (std::size_t index = 0; index < components.names().size(); index++) {
        std::optional<ItemComponent> component =
            toWire(components.names()[index], components.values()[index], enchantmentId);
        if (component.has_value()) {
            wire.push_back(std::move(*component));
        }
    }
    return wire;
}

std::optional<std::pair<std::string, NbtTag>> toStored(const ItemComponent& component,
                                                       const EnchantmentName& enchantmentName) {
    const Identifier* id = reg::dataComponents::nameOf(component.id());
    if (id == nullptr) {
        return std::nullopt;
    }
    const std::string name = id->toString();
    if (const auto* enchantments = component.as<ItemComponent::Enchantments>(); enchantments != nullptr) {
        NbtCompound stored;
        for (const auto& [enchantment, level] : enchantments->levels) {
            const Identifier* named = enchantmentName(enchantment);
            if (named == nullptr) {
                continue;
            }
            stored.putInt(named->toString(), level);
        }
        return stored.empty() ? std::nullopt : std::optional{std::pair{name, NbtTag::ofCompound(std::move(stored))}};
    }
    if (const auto* number = component.as<ItemComponent::Number>(); number != nullptr) {
        if (component.id() != dataComponent::kRarity) {
            return std::pair{name, NbtTag::ofInt(number->value)};
        }
        if (number->value < 0 || number->value >= static_cast<std::int32_t>(kRarities.size())) {
            return std::nullopt;
        }
        return std::pair{name, NbtTag::ofString(std::string{kRarities[static_cast<std::size_t>(number->value)]})};
    }
    if (const auto* colour = component.as<ItemComponent::Color>(); colour != nullptr) {
        return std::pair{name, NbtTag::ofInt(colour->rgb)};
    }
    if (const auto* flag = component.as<ItemComponent::Flag>(); flag != nullptr) {
        return std::pair{name, NbtTag::ofByte(flag->value ? 1 : 0)};
    }
    if (component.as<ItemComponent::Unit>() != nullptr) {
        return std::pair{name, NbtTag::ofCompound(NbtCompound{})};
    }
    if (const auto* text = component.as<ItemComponent::Text>(); text != nullptr) {
        return std::pair{name, wire::text::componentNbt::serialize(text->text)};
    }
    if (const auto* lore = component.as<ItemComponent::Lore>(); lore != nullptr) {
        NbtList lines{NbtType::Compound};
        for (const wire::text::Component& line : lore->lines) {
            lines.push(wire::text::componentNbt::serialize(line));
        }
        return std::pair{name, NbtTag::ofList(std::move(lines))};
    }
    if (const auto* custom = component.as<ItemComponent::CustomData>(); custom != nullptr) {
        return std::pair{name, NbtTag::ofCompound(custom->tag)};
    }
    return std::nullopt;
}

NbtCompound toStored(const std::vector<ItemComponent>& components, const EnchantmentName& enchantmentName) {
    NbtCompound stored;
    for (const ItemComponent& component : components) {
        std::optional<std::pair<std::string, NbtTag>> entry = toStored(component, enchantmentName);
        if (entry.has_value()) {
            stored.set(entry->first, std::move(entry->second));
        }
    }
    return stored;
}

} // namespace basalt::server::runtime::item::itemComponentCodecs
