#include <basalt/server/runtime/item/ItemDamageEffects.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::item {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

constexpr std::string_view kItemDamage = "minecraft:item_damage";
constexpr std::string_view kRemoveBinomial = "minecraft:remove_binomial";

[[nodiscard]] std::optional<float> numberOf(const NbtTag* tag) {
    if (tag == nullptr) {
        return std::nullopt;
    }
    if (const float* asFloat = tag->get<float>(); asFloat != nullptr) {
        return *asFloat;
    }
    if (const double* asDouble = tag->get<double>(); asDouble != nullptr) {
        return static_cast<float>(*asDouble);
    }
    if (const std::int32_t* asInt = tag->get<std::int32_t>(); asInt != nullptr) {
        return static_cast<float>(*asInt);
    }
    return std::nullopt;
}

[[nodiscard]] std::string_view stringOf(const NbtTag* tag) {
    if (tag == nullptr) {
        return {};
    }
    const std::string* text = tag->get<std::string>();
    return text == nullptr ? std::string_view{} : std::string_view{*text};
}

/// parity: `LevelBasedValue`, the shapes durability effects actually use.
[[nodiscard]] std::optional<float> levelBasedValue(const NbtTag* tag, std::int32_t level) {
    if (const std::optional<float> plain = numberOf(tag); plain.has_value()) {
        return plain;
    }
    if (tag == nullptr) {
        return std::nullopt;
    }
    const NbtCompound* compound = tag->get<NbtCompound>();
    if (compound == nullptr) {
        return std::nullopt;
    }
    const std::string_view type = stringOf(compound->find("type"));
    if (type == "minecraft:constant") {
        return numberOf(compound->find("value"));
    }
    if (type == "minecraft:linear") {
        const std::optional<float> base = numberOf(compound->find("base"));
        if (!base.has_value()) {
            return std::nullopt;
        }
        const float perLevel = numberOf(compound->find("per_level_above_first")).value_or(0.0F);
        return *base + (perLevel * static_cast<float>(level - 1));
    }
    if (type == "minecraft:fraction") {
        const std::optional<float> denominator = levelBasedValue(compound->find("denominator"), level);
        if (!denominator.has_value()) {
            return std::nullopt;
        }
        if (*denominator == 0.0F) {
            return 0.0F;
        }
        const std::optional<float> numerator = levelBasedValue(compound->find("numerator"), level);
        return numerator.has_value() ? std::optional{*numerator / *denominator} : std::nullopt;
    }
    return std::nullopt;
}

[[nodiscard]] std::vector<const NbtCompound*> conditionalEffects(const NbtCompound& definition) {
    std::vector<const NbtCompound*> found;
    const NbtCompound* effects = definition.getCompound("effects");
    if (effects == nullptr) {
        return found;
    }
    const NbtList* list = effects->getList(kItemDamage);
    if (list == nullptr) {
        return found;
    }
    for (const NbtTag& element : *list) {
        if (const NbtCompound* effect = element.get<NbtCompound>(); effect != nullptr) {
            found.push_back(effect);
        }
    }
    return found;
}

} // namespace

ItemDamageEffects::ItemDamageEffects(DefinitionOf definitionOf, ItemInTag itemInTag)
    : definitionOf_(std::move(definitionOf)), itemInTag_(std::move(itemInTag)) {}

std::int32_t ItemDamageEffects::process(const storage::StoredItemStack& stack, std::int32_t amount,
                                        random::RandomSource& random) const {
    const std::vector<std::pair<Identifier, std::int32_t>> levels = player::enchantmentLevels(stack);
    if (levels.empty()) {
        return amount;
    }
    float value = static_cast<float>(amount);
    for (const auto& [enchantment, level] : levels) {
        const NbtCompound* definition = definitionOf_ ? definitionOf_(enchantment) : nullptr;
        if (definition == nullptr) {
            continue;
        }
        for (const NbtCompound* effect : conditionalEffects(*definition)) {
            // parity: `ConditionalEffect`, of which only the `match_tool` item predicate is modelled.
            const NbtCompound* requirements = effect->getCompound("requirements");
            const NbtCompound* predicate = requirements == nullptr ? nullptr : requirements->getCompound("predicate");
            const NbtTag* items = predicate == nullptr ? nullptr : predicate->find("items");
            const std::string* wanted = items == nullptr ? nullptr : items->get<std::string>();
            if (wanted != nullptr) {
                const std::expected<Identifier, wire::common::IdentifierError> item = Identifier::parse(stack.id);
                if (!item.has_value()) {
                    continue;
                }
                if (wanted->starts_with('#')) {
                    const std::string_view tag = std::string_view{*wanted}.substr(1);
                    if (!itemInTag_ || !itemInTag_(tag, *item)) {
                        continue;
                    }
                } else {
                    const std::expected<Identifier, wire::common::IdentifierError> named = Identifier::parse(*wanted);
                    if (!named.has_value() || *named != *item) {
                        continue;
                    }
                }
            }
            const NbtCompound* applied = effect->getCompound("effect");
            if (applied == nullptr || stringOf(applied->find("type")) != kRemoveBinomial) {
                continue;
            }
            const std::optional<float> chance = levelBasedValue(applied->find("chance"), level);
            if (!chance.has_value()) {
                continue;
            }
            // parity: `RemoveBinomial.process`, the per-point roll. Its normal approximation above.
            // 128 points is left out because nothing here ever asks for that much wear at once.
            std::int32_t dropped = 0;
            const std::int32_t rolls = static_cast<std::int32_t>(value);
            for (std::int32_t roll = 0; roll < rolls; roll++) {
                if (random.nextFloat() < *chance) {
                    dropped++;
                }
            }
            value -= static_cast<float>(dropped);
        }
    }
    return static_cast<std::int32_t>(value);
}

} // namespace basalt::server::runtime::item
