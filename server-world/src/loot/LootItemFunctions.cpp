#include <basalt/server/registry/Items.hpp>
#include <basalt/server/world/loot/Loot.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::world::loot {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

constexpr std::int32_t kMaxEnchantmentLevel = 255;
constexpr std::int32_t kAdditionalEnchantmentBound = 50;
constexpr float kCostRandomSpan = 0.15F;
/// parity: `SuspiciousStewEffects.DEFAULT_DURATION`, left out of the component when it matches.
constexpr std::int32_t kDefaultStewDuration = 160;
constexpr std::int32_t kTicksPerSecond = 20;

[[nodiscard]] bool isBook(const Identifier& item) { return item == Identifier::vanilla("book"); }

/// parity: `EnchantmentHelper.getComponentType`, stored on a book and applied on anything else.
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

/// parity: `Mth.nextInt`, inclusive and short-circuiting when the ends meet.
[[nodiscard]] std::int32_t between(random::RandomSource& random, std::int32_t least, std::int32_t most) {
    return least >= most ? least : random.nextInt(most - least + 1) + least;
}

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
    // parity: `getAvailableEnchantmentResults`, the highest level whose cost band holds the cost.
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

} // namespace

LootFunction LootFunction::enchantRandomly(std::vector<std::string> options, bool onlyCompatible) {
    LootFunction built;
    built.kind_ = Kind::EnchantRandomly;
    built.options_ = std::move(options);
    built.onlyCompatible_ = onlyCompatible;
    return built;
}

LootFunction LootFunction::enchantWithLevels(NumberProvider levels, std::vector<std::string> options) {
    LootFunction built;
    built.kind_ = Kind::EnchantWithLevels;
    built.count_ = std::move(levels);
    built.options_ = std::move(options);
    return built;
}

LootFunction LootFunction::setEnchantments(std::vector<std::pair<std::string, NumberProvider>> levels, bool add) {
    LootFunction built;
    built.kind_ = Kind::SetEnchantments;
    built.levels_ = std::move(levels);
    built.add_ = add;
    return built;
}

LootFunction LootFunction::setDamage(NumberProvider damage, bool add) {
    LootFunction built;
    built.kind_ = Kind::SetDamage;
    built.count_ = std::move(damage);
    built.add_ = add;
    return built;
}

LootFunction LootFunction::setComponents(NbtCompound components, std::vector<std::string> removed) {
    LootFunction built;
    built.kind_ = Kind::SetComponents;
    built.components_ = std::make_shared<const NbtCompound>(std::move(components));
    built.removed_ = std::move(removed);
    return built;
}

LootFunction LootFunction::setStewEffect(std::vector<std::pair<std::string, NumberProvider>> effects) {
    LootFunction built;
    built.kind_ = Kind::SetStewEffect;
    built.levels_ = std::move(effects);
    return built;
}

LootFunction LootFunction::setInstrument(std::string options) {
    LootFunction built;
    built.kind_ = Kind::SetInstrument;
    built.options_ = {std::move(options)};
    return built;
}

ItemStack LootFunction::enchant(ItemStack stack, random::RandomSource& random, const LootContext& context) const {
    if (kind_ == Kind::SetEnchantments) {
        // parity: `transmuteCopy`, which keeps the count and what the book already carried.
        if (isBook(stack.item)) {
            stack.item = Identifier::vanilla("enchanted_book");
        }
        NbtCompound levels = levelsOf(stack);
        for (const auto& [name, provider] : levels_) {
            const std::int32_t base = add_ ? levels.getInt(name) : 0;
            const std::int32_t level = std::clamp(base + provider.getInt(random), 0, kMaxEnchantmentLevel);
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
        for (const EnchantmentEntry* entry : enchanting->candidates(options_)) {
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
        return stack;
    }
    // parity: `EnchantmentHelper.enchantItem`.
    const std::int32_t cost = count_.getInt(random);
    const std::vector<Pick> picks = select(*enchanting, random, stack.item, cost, options_);
    if (isBook(stack.item)) {
        stack = ItemStack{.item = Identifier::vanilla("enchanted_book"), .count = 1};
    }
    if (picks.empty()) {
        return stack;
    }
    NbtCompound levels = levelsOf(stack);
    for (const Pick& pick : picks) {
        upgrade(levels, pick.entry->id, pick.level);
    }
    storeLevels(stack, std::move(levels));
    return stack;
}

ItemStack LootFunction::setComponent(ItemStack stack, random::RandomSource& random, const LootContext& context) const {
    switch (kind_) {
    case Kind::SetDamage: {
        const std::int32_t maxDamage = server::registry::itemDurability::maxDamageOf(stack.item);
        if (maxDamage <= 0 || stack.components.find("minecraft:unbreakable") != nullptr) {
            return stack;
        }
        const float base = add_ ? 1.0F - (static_cast<float>(stack.components.getInt("minecraft:damage")) /
                                          static_cast<float>(maxDamage))
                                : 0.0F;
        const float left = 1.0F - std::clamp(count_.getFloat(random) + base, 0.0F, 1.0F);
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
        for (std::size_t index = 0; index < components_->names().size(); ++index) {
            stack.components.set(components_->names()[index], components_->values()[index]);
        }
        for (const std::string& name : removed_) {
            static_cast<void>(stack.components.remove(name));
        }
        return stack;
    case Kind::SetStewEffect: {
        if (stack.item != Identifier::vanilla("suspicious_stew") || levels_.empty()) {
            return stack;
        }
        const auto& [effect, provider] =
            levels_[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(levels_.size())))];
        std::int32_t duration = provider.getInt(random);
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
    case Kind::SetInstrument: {
        const std::string& option = options_.front();
        std::vector<Identifier> choices;
        if (option.starts_with('#')) {
            const std::expected<Identifier, wire::common::IdentifierError> tag = Identifier::parse(option.substr(1));
            if (tag.has_value() && context.tagMembers) {
                choices = context.tagMembers("instrument", *tag);
            }
        } else if (const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(option);
                   id.has_value()) {
            choices.push_back(*id);
        }
        if (choices.empty()) {
            return stack;
        }
        const Identifier& picked =
            choices[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(choices.size())))];
        stack.components.set("minecraft:instrument", NbtTag::ofString(picked.toString()));
        return stack;
    }
    default:
        return stack;
    }
}

} // namespace basalt::server::world::loot
