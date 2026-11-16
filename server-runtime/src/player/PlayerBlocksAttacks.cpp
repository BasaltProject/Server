#include <basalt/server/runtime/player/PlayerBlocksAttacks.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string_view>

namespace basalt::server::runtime::player::blocksAttacks {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

constexpr std::string_view kKey = "minecraft:blocks_attacks";
constexpr std::string_view kShield = "minecraft:shield";
constexpr float kAngleDefault = 90.0F;

[[nodiscard]] std::optional<HolderSet> holderSetOf(const NbtCompound& source, std::string_view key) {
    const NbtTag* tag = source.find(key);
    if (tag == nullptr) {
        return std::nullopt;
    }
    HolderSet set;
    if (const std::string* one = tag->get<std::string>(); one != nullptr) {
        set.entries.push_back(*one);
    } else if (const NbtList* list = tag->get<NbtList>(); list != nullptr) {
        for (const NbtTag& entry : *list) {
            if (const std::string* text = entry.get<std::string>(); text != nullptr) {
                set.entries.push_back(*text);
            }
        }
    }
    return set;
}

[[nodiscard]] std::optional<std::string> soundOf(const NbtCompound& source, std::string_view key) {
    const NbtTag* tag = source.find(key);
    if (tag == nullptr) {
        return std::nullopt;
    }
    if (const std::string* id = tag->get<std::string>(); id != nullptr) {
        return *id;
    }
    if (const NbtCompound* inline_ = tag->get<NbtCompound>(); inline_ != nullptr) {
        return std::string{inline_->getString("sound_id")};
    }
    return std::nullopt;
}

[[nodiscard]] std::string withoutNamespace(std::string_view text) {
    return std::string{text.starts_with("minecraft:") ? text.substr(10) : text};
}

/// parity: `Items.SHIELD`'s delayed `BLOCKS_ATTACKS`.
[[nodiscard]] Component shieldDefault() {
    Component shield;
    shield.blockDelaySeconds = 0.25F;
    shield.itemDamage = ItemDamageFunction{.threshold = 3.0F, .base = 1.0F, .factor = 1.0F};
    shield.bypassedBy = HolderSet{{"#minecraft:bypasses_shield"}};
    shield.blockSound = "minecraft:item.shield.block";
    shield.disableSound = "minecraft:item.shield.break";
    return shield;
}

/// parity: `BlocksAttacks.CODEC`.
[[nodiscard]] Component parse(const NbtCompound& source) {
    Component component;
    component.blockDelaySeconds = std::max(0.0F, source.getFloat("block_delay_seconds", 0.0F));
    component.disableCooldownScale = std::max(0.0F, source.getFloat("disable_cooldown_scale", 1.0F));
    if (const NbtList* reductions = source.getList("damage_reductions"); reductions != nullptr) {
        component.damageReductions.clear();
        for (const NbtTag& entry : *reductions) {
            const NbtCompound* reduction = entry.get<NbtCompound>();
            if (reduction == nullptr) {
                continue;
            }
            component.damageReductions.push_back(DamageReduction{
                .horizontalBlockingAngle = reduction->getFloat("horizontal_blocking_angle", kAngleDefault),
                .type = holderSetOf(*reduction, "type"),
                .base = reduction->getFloat("base", 0.0F),
                .factor = reduction->getFloat("factor", 0.0F)});
        }
    }
    if (const NbtCompound* damage = source.getCompound("item_damage"); damage != nullptr) {
        component.itemDamage = ItemDamageFunction{.threshold = std::max(0.0F, damage->getFloat("threshold", 0.0F)),
                                                  .base = damage->getFloat("base", 0.0F),
                                                  .factor = damage->getFloat("factor", 0.0F)};
    }
    component.bypassedBy = holderSetOf(source, "bypassed_by");
    component.blockSound = soundOf(source, "block_sound");
    component.disableSound = soundOf(source, "disabled_sound");
    return component;
}

} // namespace

std::optional<Component> of(const storage::StoredItemStack& stack) {
    if (const NbtCompound* explicitly = stack.components.getCompound(kKey); explicitly != nullptr) {
        Component component = parse(*explicitly);
        component.blockSound = component.blockSound.transform(withoutNamespace);
        component.disableSound = component.disableSound.transform(withoutNamespace);
        return component;
    }
    if (stack.components.contains("!" + std::string{kKey}) || stack.id != kShield) {
        return std::nullopt;
    }
    Component shield = shieldDefault();
    shield.blockSound = withoutNamespace(*shield.blockSound);
    shield.disableSound = withoutNamespace(*shield.disableSound);
    return shield;
}

bool contains(const HolderSet& set, const world::realDamageTypeTags::TagMembers* tags,
              const std::optional<Identifier>& damageType) {
    if (!damageType.has_value()) {
        return false;
    }
    for (const std::string& entry : set.entries) {
        const bool isTag = entry.starts_with('#');
        const std::expected<Identifier, wire::common::IdentifierError> id =
            Identifier::parse(isTag ? std::string_view{entry}.substr(1) : std::string_view{entry});
        if (!id.has_value()) {
            continue;
        }
        if (isTag ? tags != nullptr && world::realDamageTypeTags::isIn(*tags, *id, *damageType) : *id == *damageType) {
            return true;
        }
    }
    return false;
}

std::int32_t blockDelayTicks(const Component& component) {
    return static_cast<std::int32_t>(std::floor((component.blockDelaySeconds * 20.0F) + 0.5F));
}

float resolveBlockedDamage(const Component& component, const world::realDamageTypeTags::TagMembers* tags,
                           const std::optional<Identifier>& damageType, const float dealt, const double angle) {
    constexpr auto kRadians = static_cast<float>(std::numbers::pi / 180.0);
    float blocked = 0.0F;
    for (const DamageReduction& reduction : component.damageReductions) {
        // parity: `BlocksAttacks.DamageReduction.resolve`.
        if (angle > static_cast<double>(kRadians * reduction.horizontalBlockingAngle)) {
            continue;
        }
        if (reduction.type.has_value() && !contains(*reduction.type, tags, damageType)) {
            continue;
        }
        blocked += std::clamp(reduction.base + (reduction.factor * dealt), 0.0F, dealt);
    }
    return std::clamp(blocked, 0.0F, dealt);
}

std::int32_t itemDamageFor(const Component& component, const float blocked) {
    const ItemDamageFunction& function = component.itemDamage;
    return blocked < function.threshold
               ? 0
               : static_cast<std::int32_t>(std::floor(function.base + (function.factor * blocked)));
}

std::int32_t disableTicks(const Component& component, const float baseSeconds) {
    const float seconds = baseSeconds * component.disableCooldownScale;
    return seconds > 0.0F ? static_cast<std::int32_t>(std::floor((seconds * 20.0F) + 0.5F)) : 0;
}

} // namespace basalt::server::runtime::player::blocksAttacks
