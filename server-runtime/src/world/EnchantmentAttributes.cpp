#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/EnchantmentAttributes.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <map>
#include <string>

namespace basalt::server::runtime::world::enchantmentAttributes {
namespace {

namespace reg = server::registry;

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::protocol::play::UpdateAttributesPacket;

/// parity: the `minecraft:attribute` registry ids the enchantments of a held item can touch.
///
/// The numbers come from the registry rather than being written out here: one of them was wrong
/// (`attack_speed` was going out as `attack_knockback`), which is the mistake a hand-copied table
/// makes and nobody spots by reading.
constexpr std::array<std::string_view, 6> kAttributes{
    "minecraft:mining_efficiency", "minecraft:block_break_speed",      "minecraft:attack_damage",
    "minecraft:attack_speed",      "minecraft:submerged_mining_speed", "minecraft:sweeping_damage_ratio"};

/// parity: `Enchantment.definition().slots()`, of which only the main hand is held.
constexpr std::array<std::string_view, 3> kMainHandSlots{"mainhand", "hand", "any"};

[[nodiscard]] std::string_view stringOf(const NbtTag* tag) {
    if (tag == nullptr) {
        return {};
    }
    const std::string* text = tag->get<std::string>();
    return text == nullptr ? std::string_view{} : std::string_view{*text};
}

[[nodiscard]] bool appliesToMainHand(const NbtCompound& definition) {
    const NbtList* slots = definition.getList("slots");
    if (slots == nullptr) {
        return false;
    }
    for (const NbtTag& slot : *slots) {
        const std::string_view name = stringOf(&slot);
        if (std::ranges::find(kMainHandSlots, name) != kMainHandSlots.end()) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::vector<const NbtCompound*> attributeEffects(const NbtCompound& definition) {
    std::vector<const NbtCompound*> found;
    const NbtCompound* effects = definition.getCompound("effects");
    if (effects == nullptr) {
        return found;
    }
    const NbtList* attributes = effects->getList("minecraft:attributes");
    if (attributes == nullptr) {
        return found;
    }
    for (const NbtTag& element : *attributes) {
        if (const NbtCompound* effect = element.get<NbtCompound>(); effect != nullptr) {
            found.push_back(effect);
        }
    }
    return found;
}

/// parity: `LevelBasedValue`, of which enchantments use these three shapes.
[[nodiscard]] double amountOf(const NbtCompound* amount, std::int32_t level) {
    if (amount == nullptr) {
        return 0.0;
    }
    const double base = amount->getDouble("base");
    const std::string_view type = amount->getString("type");
    if (type == "minecraft:levels_squared") {
        return static_cast<double>(level * level) + amount->getDouble("added");
    }
    if (type == "minecraft:linear") {
        return base + (amount->getDouble("per_level_above_first") * static_cast<double>(level - 1));
    }
    const double value = amount->getDouble("value");
    return value != 0.0 ? value : base;
}

[[nodiscard]] UpdateAttributesPacket::Operation operationOf(std::string_view name) {
    if (name == "add_multiplied_base") {
        return UpdateAttributesPacket::Operation::AddMultipliedBase;
    }
    if (name == "add_multiplied_total") {
        return UpdateAttributesPacket::Operation::AddMultipliedTotal;
    }
    return UpdateAttributesPacket::Operation::AddValue;
}

/// parity: `Attributes`' own defaults for the ones a held item touches.
[[nodiscard]] double baseOf(std::string_view attribute) {
    if (attribute == "minecraft:block_break_speed" || attribute == "minecraft:attack_speed" ||
        attribute == "minecraft:attack_damage") {
        return 1.0;
    }
    return attribute == "minecraft:submerged_mining_speed" ? 0.2 : 0.0;
}

} // namespace

std::vector<UpdateAttributesPacket::AttributeSnapshot>
snapshotsFor(const std::optional<storage::StoredItemStack>& stack, const DefinitionOf& definitionOf) {
    std::map<std::string_view, std::vector<UpdateAttributesPacket::Modifier>> modifiers;
    if (stack.has_value()) {
        for (const auto& [id, level] : player::enchantmentLevels(*stack)) {
            const NbtCompound* definition = definitionOf ? definitionOf(id) : nullptr;
            if (definition == nullptr || !appliesToMainHand(*definition)) {
                continue;
            }
            for (const NbtCompound* effect : attributeEffects(*definition)) {
                const std::string_view attribute = effect->getString("attribute");
                const auto known = std::ranges::find(kAttributes, attribute);
                if (attribute.empty() || known == kAttributes.end()) {
                    continue;
                }
                // parity: `EnchantmentAttributeEffect` makes the id mandatory, so this only names one
                // for a datapack that left it out. The enchantment's path rather than its whole name:
                // a second colon would not be an identifier at all, and the modifier would be lost.
                const std::string_view named = effect->getString("id");
                const std::string wanted =
                    named.empty() ? "minecraft:enchantment." + std::string{id.path()} : std::string{named};
                const std::expected<Identifier, wire::common::IdentifierError> modifierId = Identifier::parse(wanted);
                if (!modifierId.has_value()) {
                    continue;
                }
                modifiers[*known].push_back(
                    UpdateAttributesPacket::Modifier{.id = *modifierId,
                                                     .amount = amountOf(effect->getCompound("amount"), level),
                                                     .operation = operationOf(effect->getString("operation"))});
            }
        }
    }

    std::vector<UpdateAttributesPacket::AttributeSnapshot> snapshots;
    snapshots.reserve(kAttributes.size());
    for (const std::string_view attribute : kAttributes) {
        const std::optional<std::int32_t> protocolId = reg::attributeIds::protocolId(attribute);
        if (!protocolId.has_value()) {
            continue;
        }
        const auto found = modifiers.find(attribute);
        snapshots.push_back(UpdateAttributesPacket::AttributeSnapshot{
            .attribute = *protocolId,
            .base = baseOf(attribute),
            .modifiers = found == modifiers.end() ? std::vector<UpdateAttributesPacket::Modifier>{} : found->second});
    }
    return snapshots;
}

} // namespace basalt::server::runtime::world::enchantmentAttributes
