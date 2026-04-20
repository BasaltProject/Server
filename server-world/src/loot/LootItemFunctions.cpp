#include "LootParts.hpp"

#include <basalt/server/registry/Items.hpp>
#include <basalt/server/world/crafting/Crafting.hpp>
#include <basalt/server/world/crafting/SpecialCrafting.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::world::loot {
namespace {

/// parity: `DataComponents.ADDITIONAL_TRADE_COST`.
constexpr std::string_view kAdditionalTradeCost = "minecraft:additional_trade_cost";

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

constexpr std::int32_t kMaxEnchantmentLevel = 255;
constexpr std::int32_t kAdditionalEnchantmentBound = 50;
constexpr float kCostRandomSpan = 0.15F;
/// parity: `SuspiciousStewEffects.DEFAULT_DURATION`.
constexpr std::int32_t kDefaultStewDuration = 160;
constexpr std::int32_t kTicksPerSecond = 20;

[[nodiscard]] bool isBook(const Identifier& item) { return item == Identifier::vanilla("book"); }

/// parity: `EnchantmentHelper.getComponentType`.
[[nodiscard]] std::string_view enchantmentKey(const Identifier& item) {
    return item == Identifier::vanilla("enchanted_book") ? "minecraft:stored_enchantments" : "minecraft:enchantments";
}

[[nodiscard]] NbtCompound levelsOf(const ItemStack& stack) {
    const NbtCompound* levels = stack.components.getCompound(enchantmentKey(stack.item));
    return levels == nullptr ? NbtCompound{} : *levels;
}

void storeLevels(ItemStack& stack, NbtCompound levels) {
    const std::string_view key = enchantmentKey(stack.item);
    if (levels.empty()) {
        static_cast<void>(stack.components.remove(key));
        return;
    }
    stack.components.set(key, NbtTag::ofCompound(std::move(levels)));
}

/// parity: `ItemEnchantments.Mutable.upgrade`, which only ever raises a level.
void upgrade(NbtCompound& levels, const Identifier& enchantment, std::int32_t level) {
    const std::string key = enchantment.toString();
    levels.putInt(key, std::max(levels.getInt(key), level));
}

using lootMath::between;

struct Pick {
    const EnchantmentEntry* entry = nullptr;
    std::int32_t level = 0;
};

/// parity: `WeightedRandom.getRandomItem`, which draws nothing when every weight is zero.
[[nodiscard]] std::optional<Pick> weighted(const std::vector<Pick>& from, random::RandomSource& random) {
    std::int32_t total = 0;
    for (const Pick& pick : from) {
        total += pick.entry->weight;
    }
    if (total <= 0) {
        return std::nullopt;
    }
    std::int32_t index = random.nextInt(total);
    for (const Pick& pick : from) {
        index -= pick.entry->weight;
        if (index < 0) {
            return pick;
        }
    }
    return std::nullopt;
}

/// parity: `EnchantmentHelper.selectEnchantment`.
[[nodiscard]] std::vector<Pick> select(const Enchanting& enchanting, random::RandomSource& random,
                                       const Identifier& item, std::int32_t cost,
                                       const std::vector<std::string>& options) {
    std::vector<Pick> results;
    const std::optional<std::int32_t> enchantability =
        enchanting.enchantability ? enchanting.enchantability(item) : std::nullopt;
    if (!enchantability.has_value()) {
        return results;
    }
    cost += 1 + random.nextInt((*enchantability / 4) + 1) + random.nextInt((*enchantability / 4) + 1);
    const float span = (random.nextFloat() + random.nextFloat() - 1.0F) * kCostRandomSpan;
    // parity: `Math.round(float)`, half up, then clamped to at least one.
    cost = std::max(
        1, static_cast<std::int32_t>(std::floor(static_cast<float>(cost) + (static_cast<float>(cost) * span) + 0.5F)));
    // parity: `getAvailableEnchantmentResults`.
    std::vector<Pick> available;
    const bool book = isBook(item);
    for (const EnchantmentEntry* entry : enchanting.candidates(options)) {
        if (!book && !(enchanting.primary && enchanting.primary(entry->id, item))) {
            continue;
        }
        for (std::int32_t level = entry->maxLevel; level >= 1; --level) {
            if (cost >= entry->minCost.at(level) && cost <= entry->maxCost.at(level)) {
                available.push_back(Pick{.entry = entry, .level = level});
                break;
            }
        }
    }
    if (available.empty()) {
        return results;
    }
    if (const std::optional<Pick> first = weighted(available, random); first.has_value()) {
        results.push_back(*first);
    }
    while (random.nextInt(kAdditionalEnchantmentBound) <= cost) {
        if (!results.empty()) {
            const Identifier last = results.back().entry->id;
            std::erase_if(available, [&](const Pick& pick) {
                return !(enchanting.compatible && enchanting.compatible(last, pick.entry->id));
            });
        }
        if (available.empty()) {
            break;
        }
        if (const std::optional<Pick> next = weighted(available, random); next.has_value()) {
            results.push_back(*next);
        }
        cost /= 2;
    }
    return results;
}

/// parity: `MobEffect.isInstantaneous`, true for the `InstantenousMobEffect` family.
[[nodiscard]] bool instantaneous(std::string_view effect) {
    static constexpr std::array<std::string_view, 3> kInstant{"minecraft:instant_health", "minecraft:instant_damage",
                                                              "minecraft:saturation"};
    return std::ranges::find(kInstant, effect) != kInstant.end();
}

constexpr std::int32_t kMaxOminousAmplifier = 4;

/// parity: `DyeColor.values()`, in ordinal order.
constexpr std::array<std::string_view, 16> kDyeColours{"white", "orange", "magenta",    "light_blue", "yellow", "lime",
                                                       "pink",  "gray",   "light_gray", "cyan",       "purple", "blue",
                                                       "brown", "green",  "red",        "black"};

[[nodiscard]] std::vector<Identifier> membersOf(const std::vector<std::string>& options, std::string_view registry,
                                                const LootContext& context) {
    std::vector<Identifier> members;
    for (const std::string& option : options) {
        if (option.starts_with('#')) {
            const std::expected<Identifier, wire::common::IdentifierError> tag = Identifier::parse(option.substr(1));
            if (tag.has_value() && context.tagMembers) {
                for (const Identifier& member : context.tagMembers(registry, *tag)) {
                    members.push_back(member);
                }
            }
        } else if (const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(option);
                   id.has_value()) {
            members.push_back(*id);
        }
    }
    return members;
}

} // namespace

std::map<std::string, std::int32_t, std::less<>> enchantmentsOf(const ItemStack& stack) {
    std::map<std::string, std::int32_t, std::less<>> levels;
    if (const NbtCompound* held = stack.components.getCompound("minecraft:enchantments"); held != nullptr) {
        for (std::size_t index = 0; index < held->names().size(); ++index) {
            levels.emplace(held->names()[index], held->getInt(held->names()[index]));
        }
    }
    return levels;
}

LootFunction LootFunction::enchantRandomly(std::vector<std::string> options, bool onlyCompatible) {
    LootFunction built;
    built.kind_ = Kind::EnchantRandomly;
    built.onlyCompatible_ = onlyCompatible;
    built.parts_ = std::make_shared<const Parts>(Parts{.options = std::move(options)});
    return built;
}

LootFunction LootFunction::enchantWithLevels(IntProvider levels, std::vector<std::string> options) {
    LootFunction built;
    built.kind_ = Kind::EnchantWithLevels;
    built.parts_ = std::make_shared<const Parts>(Parts{.count = std::move(levels), .options = std::move(options)});
    return built;
}

LootFunction LootFunction::setEnchantments(std::vector<std::pair<std::string, IntProvider>> levels, bool add) {
    LootFunction built;
    built.kind_ = Kind::SetEnchantments;
    built.add_ = add;
    built.parts_ = std::make_shared<const Parts>(Parts{.levels = std::move(levels)});
    return built;
}

LootFunction LootFunction::setDamage(FloatProvider damage, bool add) {
    LootFunction built;
    built.kind_ = Kind::SetDamage;
    built.add_ = add;
    built.parts_ = std::make_shared<const Parts>(Parts{.fraction = std::move(damage)});
    return built;
}

LootFunction LootFunction::setComponents(NbtCompound components, std::vector<std::string> removed) {
    LootFunction built;
    built.kind_ = Kind::SetComponents;
    built.parts_ =
        std::make_shared<const Parts>(Parts{.components = std::move(components), .removed = std::move(removed)});
    return built;
}

LootFunction LootFunction::setStewEffect(std::vector<std::pair<std::string, IntProvider>> effects) {
    LootFunction built;
    built.kind_ = Kind::SetStewEffect;
    built.parts_ = std::make_shared<const Parts>(Parts{.levels = std::move(effects)});
    return built;
}

LootFunction LootFunction::setInstrument(std::vector<std::string> options) {
    LootFunction built;
    built.kind_ = Kind::SetInstrument;
    built.parts_ = std::make_shared<const Parts>(Parts{.options = std::move(options)});
    return built;
}

ItemStack LootFunction::enchant(ItemStack stack, random::RandomSource& random, const LootContext& context) const {
    if (kind_ == Kind::SetEnchantments) {
        // parity: `transmuteCopy`, which keeps the count and what the book already carried.
        if (isBook(stack.item)) {
            stack.item = Identifier::vanilla("enchanted_book");
        }
        NbtCompound levels = levelsOf(stack);
        for (const auto& [name, provider] : parts().levels) {
            const std::int32_t base = add_ ? levels.getInt(name) : 0;
            const std::int32_t level = std::clamp(base + provider.get(random, context), 0, kMaxEnchantmentLevel);
            if (level <= 0) {
                static_cast<void>(levels.remove(name));
            } else {
                levels.putInt(name, level);
            }
        }
        storeLevels(stack, std::move(levels));
        return stack;
    }
    const Enchanting* enchanting = context.enchanting;
    if (enchanting == nullptr || !enchanting->candidates) {
        return stack;
    }
    if (kind_ == Kind::EnchantRandomly) {
        const bool book = isBook(stack.item);
        const bool checked = !book && onlyCompatible_;
        std::vector<const EnchantmentEntry*> compatible;
        for (const EnchantmentEntry* entry : enchanting->candidates(parts().options)) {
            if (!checked || (enchanting->supports && enchanting->supports(entry->id, stack.item))) {
                compatible.push_back(entry);
            }
        }
        if (compatible.empty()) {
            return stack;
        }
        const EnchantmentEntry* picked =
            compatible[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(compatible.size())))];
        const std::int32_t level = between(random, 1, picked->maxLevel);
        // parity: `new ItemStack(Items.ENCHANTED_BOOK)`, a fresh single book.
        if (book) {
            stack = ItemStack{.item = Identifier::vanilla("enchanted_book"), .count = 1};
        }
        NbtCompound levels = levelsOf(stack);
        upgrade(levels, picked->id, level);
        storeLevels(stack, std::move(levels));
        // parity: `EnchantRandomlyFunction.enchantItem`'s price, rolled after the level.
        if (additionalCost_ && context.additionalCostAllowed) {
            stack.components.putInt(kAdditionalTradeCost, 2 + random.nextInt(5 + (level * 10)) + (3 * level));
        }
        return stack;
    }
    // parity: `EnchantmentHelper.enchantItem`.
    const std::int32_t cost = parts().count.get(random, context);
    const std::vector<Pick> picks = select(*enchanting, random, stack.item, cost, parts().options);
    if (isBook(stack.item)) {
        stack = ItemStack{.item = Identifier::vanilla("enchanted_book"), .count = 1};
    }
    if (!picks.empty()) {
        NbtCompound levels = levelsOf(stack);
        for (const Pick& pick : picks) {
            upgrade(levels, pick.entry->id, pick.level);
        }
        storeLevels(stack, std::move(levels));
    }
    // parity: `EnchantWithLevelsFunction.run` prices the levels it spent into a trade.
    if (additionalCost_ && context.additionalCostAllowed && stack.count > 0 && cost > 0) {
        stack.components.putInt(kAdditionalTradeCost, cost);
    }
    return stack;
}

ItemStack LootFunction::setComponent(ItemStack stack, random::RandomSource& random, const LootContext& context) const {
    const Parts& part = parts();
    switch (kind_) {
    case Kind::SetDamage: {
        const std::int32_t maxDamage = server::registry::itemDurability::maxDamageOf(stack.item);
        if (maxDamage <= 0 || stack.components.find("minecraft:unbreakable") != nullptr) {
            return stack;
        }
        const float base = add_ ? 1.0F - (static_cast<float>(stack.components.getInt("minecraft:damage")) /
                                          static_cast<float>(maxDamage))
                                : 0.0F;
        const float left = 1.0F - wire::common::mth::clamp(part.fraction.get(random, context) + base, 0.0F, 1.0F);
        const auto damage = static_cast<std::int32_t>(std::floor(left * static_cast<float>(maxDamage)));
        // parity: a damage of zero matches the prototype, so the patch drops it.
        if (damage <= 0) {
            static_cast<void>(stack.components.remove("minecraft:damage"));
        } else {
            stack.components.putInt("minecraft:damage", damage);
        }
        return stack;
    }
    case Kind::SetComponents:
        for (std::size_t index = 0; index < part.components.names().size(); ++index) {
            stack.components.set(part.components.names()[index], part.components.values()[index]);
        }
        for (const std::string& name : part.removed) {
            static_cast<void>(stack.components.remove(name));
        }
        return stack;
    case Kind::SetStewEffect: {
        if (stack.item != Identifier::vanilla("suspicious_stew") || part.levels.empty()) {
            return stack;
        }
        const auto& [effect, provider] =
            part.levels[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(part.levels.size())))];
        std::int32_t duration = provider.get(random, context);
        if (!instantaneous(effect)) {
            duration *= kTicksPerSecond;
        }
        NbtCompound entry;
        entry.putString("id", effect);
        if (duration != kDefaultStewDuration) {
            entry.putInt("duration", duration);
        }
        const NbtList* existing = stack.components.getList("minecraft:suspicious_stew_effects");
        NbtList effects = existing == nullptr ? NbtList{wire::nbt::NbtType::Compound} : *existing;
        static_cast<void>(effects.push(NbtTag::ofCompound(std::move(entry))));
        stack.components.set("minecraft:suspicious_stew_effects", NbtTag::ofList(std::move(effects)));
        return stack;
    }
    case Kind::SetInstrument:
    case Kind::SetRandomPotion: {
        const bool instrument = kind_ == Kind::SetInstrument;
        // parity: `Registry.getRandom` over every potion when the function named none.
        const std::vector<Identifier> choices =
            !instrument && add_
                ? (context.registryEntries ? context.registryEntries("potion") : std::vector<Identifier>{})
                : membersOf(part.options, instrument ? "instrument" : "potion", context);
        if (choices.empty()) {
            return stack;
        }
        const Identifier& picked =
            choices[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(choices.size())))];
        if (instrument) {
            stack.components.set("minecraft:instrument", NbtTag::ofString(picked.toString()));
            return stack;
        }
        // parity: `PotionContents.withPotion`, the rest of what the bottle held kept.
        const NbtCompound* held = stack.components.getCompound("minecraft:potion_contents");
        NbtCompound contents = held == nullptr ? NbtCompound{} : *held;
        contents.putString("potion", picked.toString());
        stack.components.set("minecraft:potion_contents", NbtTag::ofCompound(std::move(contents)));
        return stack;
    }
    // parity: `SetOminousBottleAmplifierFunction`, the roll clamped to the five levels.
    case Kind::SetOminousBottleAmplifier:
        stack.components.putInt("minecraft:ominous_bottle_amplifier",
                                std::clamp(part.count.get(random, context), 0, kMaxOminousAmplifier));
        return stack;
    // parity: `SetRandomDyesFunction`.
    case Kind::SetRandomDyes: {
        const std::int32_t rolls = part.count.get(random, context);
        if (rolls <= 0) {
            return stack;
        }
        std::vector<std::int32_t> dyes;
        for (std::int32_t index = 0; index < rolls; ++index) {
            const std::string_view colour =
                kDyeColours[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kDyeColours.size())))];
            dyes.push_back(crafting::hooks::dyeColourOf(Identifier::vanilla(std::string{colour} + "_dye")).value_or(0));
        }
        const NbtTag* current = stack.components.find(crafting::specialRecipes::kDyedColor);
        const std::int32_t* rgb = current == nullptr ? nullptr : current->get<std::int32_t>();
        stack.count = 1;
        stack.components.set(crafting::specialRecipes::kDyedColor,
                             NbtTag::ofInt(crafting::specialRecipes::mixDyes(
                                 rgb == nullptr ? std::nullopt : std::optional{*rgb}, dyes)));
        return stack;
    }
    default:
        return stack;
    }
}

} // namespace basalt::server::world::loot
