#include "LootComponentEdit.hpp"
#include "LootParts.hpp"

#include <basalt/server/registry/Items.hpp>

#include <algorithm>
#include <array>
#include <utility>

namespace basalt::server::world::loot {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr std::string_view kLore = "minecraft:lore";
constexpr std::string_view kTooltipDisplay = "minecraft:tooltip_display";
constexpr std::string_view kCustomData = "minecraft:custom_data";
constexpr std::string_view kCustomModelData = "minecraft:custom_model_data";
constexpr std::string_view kWrittenBook = "minecraft:written_book_content";
constexpr std::string_view kWritableBook = "minecraft:writable_book_content";
constexpr std::string_view kFireworkExplosion = "minecraft:firework_explosion";
constexpr std::string_view kFireworks = "minecraft:fireworks";
constexpr std::string_view kBannerPatterns = "minecraft:banner_patterns";
constexpr std::string_view kContainerLoot = "minecraft:container_loot";
constexpr std::string_view kProfile = "minecraft:profile";
constexpr std::string_view kAttributeModifiers = "minecraft:attribute_modifiers";
/// parity: `SetLoreFunction` and `SetFireworksFunction`, `ListOperation.codec(256)`.
constexpr std::int32_t kMaxLore = 256;
constexpr std::int32_t kMaxExplosions = 256;
/// parity: `WritableBookContent.PAGES_CODEC`'s hundred pages.
constexpr std::int32_t kMaxWritablePages = 100;

[[nodiscard]] std::vector<NbtTag> elementsOf(const NbtTag* tag) {
    std::vector<NbtTag> elements;
    if (tag == nullptr) {
        return elements;
    }
    if (const auto* list = tag->get<NbtList>(); list != nullptr) {
        return list->elements();
    }
    if (const auto* ints = tag->get<std::vector<std::int32_t>>(); ints != nullptr) {
        for (const std::int32_t value : *ints) {
            elements.push_back(NbtTag::ofInt(value));
        }
    } else if (const auto* bytes = tag->get<std::vector<std::int8_t>>(); bytes != nullptr) {
        for (const std::int8_t value : *bytes) {
            elements.push_back(NbtTag::ofByte(value));
        }
    }
    return elements;
}

/// parity: `NbtOps.createList`.
[[nodiscard]] NbtTag listTag(const std::vector<NbtTag>& elements) {
    if (!elements.empty() && std::ranges::all_of(elements, [](const NbtTag& e) { return e.type() == NbtType::Int; })) {
        std::vector<std::int32_t> ints;
        for (const NbtTag& element : elements) {
            ints.push_back(*element.get<std::int32_t>());
        }
        return NbtTag::ofIntArray(std::move(ints));
    }
    if (!elements.empty() && std::ranges::all_of(elements, [](const NbtTag& e) { return e.type() == NbtType::Byte; })) {
        std::vector<std::int8_t> bytes;
        for (const NbtTag& element : elements) {
            bytes.push_back(*element.get<std::int8_t>());
        }
        return NbtTag::ofByteArray(std::move(bytes));
    }
    NbtList list;
    for (const NbtTag& element : elements) {
        static_cast<void>(list.push(element));
    }
    return NbtTag::ofList(std::move(list));
}

/// Writes a list field, left out when empty as `optionalFieldOf` leaves out its default.
void putList(NbtCompound& into, std::string_view key, const std::vector<NbtTag>& elements) {
    if (elements.empty()) {
        static_cast<void>(into.remove(key));
    } else {
        into.set(key, listTag(elements));
    }
}

[[nodiscard]] NbtCompound compoundAt(const ItemStack& stack, std::string_view key) {
    const NbtCompound* found = stack.components.getCompound(key);
    return found == nullptr ? NbtCompound{} : *found;
}

/// parity: `CompoundTag.merge`, compounds merged key by key and anything else replaced.
void merge(NbtCompound& into, const NbtCompound& from) {
    for (std::size_t index = 0; index < from.names().size(); ++index) {
        const std::string& key = from.names()[index];
        const NbtTag& value = from.values()[index];
        const NbtCompound* nested = value.get<NbtCompound>();
        const NbtCompound* existing = into.getCompound(key);
        if (nested != nullptr && existing != nullptr) {
            NbtCompound merged = *existing;
            merge(merged, *nested);
            into.set(key, NbtTag::ofCompound(std::move(merged)));
        } else {
            into.set(key, value);
        }
    }
}

[[nodiscard]] const EntityFacts* factsOf(const LootContext& context, EntityTarget target) {
    const std::optional<EntityFacts>* facts = nullptr;
    switch (target) {
    case EntityTarget::This:
        facts = &context.thisEntity;
        break;
    case EntityTarget::Attacker:
        facts = &context.attacker;
        break;
    case EntityTarget::DirectAttacker:
        facts = &context.directAttacker;
        break;
    case EntityTarget::AttackingPlayer:
        facts = &context.attackingPlayer;
        break;
    default:
        return nullptr;
    }
    return facts->has_value() ? &**facts : nullptr;
}

[[nodiscard]] std::vector<NbtTag> prototypeModifiers(const Identifier& item) {
    std::vector<NbtTag> modifiers;
    const auto add = [&modifiers](std::string_view type, std::string_view id, double amount, std::string_view slot) {
        NbtCompound entry;
        entry.putString("type", std::string{type});
        entry.putString("id", std::string{id});
        entry.putDouble("amount", amount);
        entry.putString("operation", "add_value");
        entry.putString("slot", std::string{slot});
        modifiers.push_back(NbtTag::ofCompound(std::move(entry)));
    };
    if (const server::registry::WeaponStats* weapon = server::registry::itemWeapons::of(item); weapon != nullptr) {
        add("minecraft:attack_damage", "minecraft:base_attack_damage", weapon->attackDamage, "mainhand");
        add("minecraft:attack_speed", "minecraft:base_attack_speed", weapon->attackSpeed, "mainhand");
    }
    if (const server::registry::ArmourPiece* armour = server::registry::itemArmour::of(item); armour != nullptr) {
        const std::string id = "minecraft:armor." + std::string{armour->slot};
        add("minecraft:armor", id, static_cast<double>(armour->armour), armour->slot);
        add("minecraft:armor_toughness", id, static_cast<double>(armour->toughness), armour->slot);
        if (armour->knockbackResistance > 0.0F) {
            add("minecraft:knockback_resistance", id, static_cast<double>(armour->knockbackResistance), armour->slot);
        }
    }
    return modifiers;
}

ItemStack setAttributes(const ComponentEdit& edit, ItemStack stack, random::RandomSource& random,
                        const LootContext& context) {
    std::vector<NbtTag> modifiers;
    if (!edit.replace) {
        const NbtTag* current = stack.components.find(kAttributeModifiers);
        modifiers = current != nullptr ? elementsOf(current) : prototypeModifiers(stack.item);
    }
    for (const AttributeModifierSpec& spec : edit.modifiers) {
        // parity: `Util.getRandom` on the slots first, then the amount.
        const std::string& slot =
            spec.slots[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(spec.slots.size())))];
        const float amount = spec.amount.get(random, context);
        std::erase_if(modifiers, [&spec](const NbtTag& tag) {
            const NbtCompound* entry = tag.get<NbtCompound>();
            return entry != nullptr && entry->getString("type") == spec.attribute && entry->getString("id") == spec.id;
        });
        NbtCompound entry;
        entry.putString("type", spec.attribute);
        entry.putString("id", spec.id);
        entry.putDouble("amount", static_cast<double>(amount));
        entry.putString("operation", spec.operation);
        if (slot != "any") {
            entry.putString("slot", slot);
        }
        modifiers.push_back(NbtTag::ofCompound(std::move(entry)));
    }
    NbtList list{NbtType::Compound};
    for (NbtTag& modifier : modifiers) {
        static_cast<void>(list.push(std::move(modifier)));
    }
    stack.components.set(kAttributeModifiers, NbtTag::ofList(std::move(list)));
    return stack;
}

ItemStack setCustomModelData(const ComponentEdit& edit, ItemStack stack, random::RandomSource& random,
                             const LootContext& context) {
    NbtCompound data = compoundAt(stack, kCustomModelData);
    // parity: the constructor's arguments, floats rolled before colours.
    if (edit.floats.has_value()) {
        std::vector<NbtTag> rolled;
        for (const FloatProvider& provider : edit.floats->first) {
            rolled.push_back(NbtTag::ofFloat(provider.get(random, context)));
        }
        putList(data, "floats", edit.floats->second.apply(elementsOf(data.find("floats")), rolled));
    }
    if (edit.flags.has_value()) {
        putList(data, "flags", edit.flags->second.apply(elementsOf(data.find("flags")), edit.flags->first));
    }
    if (edit.strings.has_value()) {
        putList(data, "strings", edit.strings->second.apply(elementsOf(data.find("strings")), edit.strings->first));
    }
    if (edit.colors.has_value()) {
        std::vector<NbtTag> rolled;
        for (const IntProvider& provider : edit.colors->first) {
            rolled.push_back(NbtTag::ofInt(provider.get(random, context)));
        }
        putList(data, "colors", edit.colors->second.apply(elementsOf(data.find("colors")), rolled));
    }
    stack.components.set(kCustomModelData, NbtTag::ofCompound(std::move(data)));
    return stack;
}

/// parity: `WrittenBookContent.EMPTY`, the book an item without one is updated from.
[[nodiscard]] NbtCompound writtenBookOf(const ItemStack& stack) {
    if (const NbtCompound* held = stack.components.getCompound(kWrittenBook); held != nullptr) {
        return *held;
    }
    NbtCompound empty;
    NbtCompound title;
    title.putString("raw", "");
    empty.set("title", NbtTag::ofCompound(std::move(title)));
    empty.putString("author", "");
    empty.putBoolean("resolved", true);
    return empty;
}

ItemStack setFireworkExplosion(const ComponentEdit& edit, ItemStack stack) {
    NbtCompound explosion = compoundAt(stack, kFireworkExplosion);
    if (explosion.find("shape") == nullptr) {
        explosion.putString("shape", "small_ball");
    }
    for (std::size_t index = 0; index < edit.compound.names().size(); ++index) {
        const std::string& key = edit.compound.names()[index];
        const NbtTag& value = edit.compound.values()[index];
        if (key == "has_trail" || key == "has_twinkle") {
            if (value.get<std::int8_t>() != nullptr && *value.get<std::int8_t>() != 0) {
                explosion.set(key, value);
            } else {
                static_cast<void>(explosion.remove(key));
            }
        } else if (key == "colors" || key == "fade_colors") {
            putList(explosion, key, elementsOf(&value));
        } else {
            explosion.set(key, value);
        }
    }
    stack.components.set(kFireworkExplosion, NbtTag::ofCompound(std::move(explosion)));
    return stack;
}

ItemStack setFireworks(const ComponentEdit& edit, ItemStack stack) {
    NbtCompound fireworks = compoundAt(stack, kFireworks);
    if (edit.flightDuration.has_value()) {
        if (*edit.flightDuration == 0) {
            static_cast<void>(fireworks.remove("flight_duration"));
        } else {
            fireworks.putByte("flight_duration", static_cast<std::int8_t>(*edit.flightDuration));
        }
    }
    if (edit.hasOperation) {
        const std::vector<NbtTag> explosions =
            edit.operation.apply(elementsOf(fireworks.find("explosions")), edit.values, kMaxExplosions);
        NbtList list{NbtType::Compound};
        for (const NbtTag& explosion : explosions) {
            static_cast<void>(list.push(explosion));
        }
        if (list.empty()) {
            static_cast<void>(fireworks.remove("explosions"));
        } else {
            fireworks.set("explosions", NbtTag::ofList(std::move(list)));
        }
    }
    stack.components.set(kFireworks, NbtTag::ofCompound(std::move(fireworks)));
    return stack;
}

ItemStack setBookPages(const ComponentEdit& edit, ItemStack stack) {
    if (edit.op == ComponentEdit::Op::SetWritableBookPages) {
        NbtCompound book = compoundAt(stack, kWritableBook);
        putList(book, "pages", edit.operation.apply(elementsOf(book.find("pages")), edit.values, kMaxWritablePages));
        stack.components.set(kWritableBook, NbtTag::ofCompound(std::move(book)));
        return stack;
    }
    NbtCompound book = writtenBookOf(stack);
    putList(book, "pages", edit.operation.apply(elementsOf(book.find("pages")), edit.values));
    // parity: `withReplacedPages` leaves the book unresolved.
    static_cast<void>(book.remove("resolved"));
    stack.components.set(kWrittenBook, NbtTag::ofCompound(std::move(book)));
    return stack;
}

ItemStack setBookCover(const ComponentEdit& edit, ItemStack stack) {
    NbtCompound book = writtenBookOf(stack);
    for (std::size_t index = 0; index < edit.compound.names().size(); ++index) {
        const std::string& key = edit.compound.names()[index];
        const NbtTag& value = edit.compound.values()[index];
        if (key == "generation" && value.get<std::int32_t>() != nullptr && *value.get<std::int32_t>() == 0) {
            static_cast<void>(book.remove(key));
        } else {
            book.set(key, value);
        }
    }
    stack.components.set(kWrittenBook, NbtTag::ofCompound(std::move(book)));
    return stack;
}

ItemStack toggleTooltips(const ComponentEdit& edit, ItemStack stack) {
    NbtCompound display = compoundAt(stack, kTooltipDisplay);
    std::vector<NbtTag> hidden = elementsOf(display.find("hidden_components"));
    for (const auto& [component, shown] : edit.toggles) {
        const auto found = std::ranges::find_if(hidden, [&component](const NbtTag& tag) {
            const std::string* name = tag.get<std::string>();
            return name != nullptr && *name == component;
        });
        if (shown && found != hidden.end()) {
            hidden.erase(found);
        } else if (!shown && found == hidden.end()) {
            hidden.push_back(NbtTag::ofString(component));
        }
    }
    putList(display, "hidden_components", hidden);
    // parity: `TooltipDisplay.DEFAULT` is every item's own, so the patch drops it.
    if (display.names().empty()) {
        static_cast<void>(stack.components.remove(kTooltipDisplay));
    } else {
        stack.components.set(kTooltipDisplay, NbtTag::ofCompound(std::move(display)));
    }
    return stack;
}

} // namespace

std::vector<NbtTag> ListOperation::apply(const std::vector<NbtTag>& original, const std::vector<NbtTag>& replacement,
                                         std::int32_t maxSize) const {
    const auto size = static_cast<std::int32_t>(original.size());
    const auto added = static_cast<std::int32_t>(replacement.size());
    std::vector<NbtTag> result;
    switch (mode) {
    case Mode::ReplaceAll:
        return replacement;
    case Mode::ReplaceSection: {
        if (offset > size) {
            return original;
        }
        result.assign(original.begin(), original.begin() + offset);
        result.insert(result.end(), replacement.begin(), replacement.end());
        const std::int32_t resume = offset + sectionSize.value_or(added);
        if (resume < size) {
            result.insert(result.end(), original.begin() + resume, original.end());
        }
        return static_cast<std::int32_t>(result.size()) > maxSize ? original : result;
    }
    case Mode::Insert:
        if (offset > size || size + added > maxSize) {
            return original;
        }
        result.assign(original.begin(), original.begin() + offset);
        result.insert(result.end(), replacement.begin(), replacement.end());
        result.insert(result.end(), original.begin() + offset, original.end());
        return result;
    case Mode::Append:
    default:
        if (size + added > maxSize) {
            return original;
        }
        result = original;
        result.insert(result.end(), replacement.begin(), replacement.end());
        return result;
    }
}

ItemStack applyComponentEdit(const ComponentEdit& edit, ItemStack stack, random::RandomSource& random,
                             const LootContext& context) {
    switch (edit.op) {
    case ComponentEdit::Op::ToggleTooltips:
        return toggleTooltips(edit, std::move(stack));
    // parity: `CustomData.update`, the tag merged in and an empty result no component at all.
    case ComponentEdit::Op::SetCustomData: {
        NbtCompound data = compoundAt(stack, kCustomData);
        merge(data, edit.compound);
        if (data.names().empty()) {
            static_cast<void>(stack.components.remove(kCustomData));
        } else {
            stack.components.set(kCustomData, NbtTag::ofCompound(std::move(data)));
        }
        return stack;
    }
    case ComponentEdit::Op::SetCustomModelData:
        return setCustomModelData(edit, std::move(stack), random, context);
    // parity: `SetLoreFunction.updateLore`.
    case ComponentEdit::Op::SetLore: {
        std::vector<NbtTag> given = edit.values;
        if (edit.resolvesAgainstEntity && context.resolveText) {
            if (const std::optional<std::string> holder = scoreHolderOf(context, edit.entity); holder.has_value()) {
                for (NbtTag& line : given) {
                    if (std::optional<NbtTag> resolved = context.resolveText(line, *holder); resolved.has_value()) {
                        line = std::move(*resolved);
                    }
                }
            }
        }
        const std::vector<NbtTag> lines =
            edit.operation.apply(elementsOf(stack.components.find(kLore)), given, kMaxLore);
        if (lines.empty()) {
            static_cast<void>(stack.components.remove(kLore));
        } else {
            NbtList list;
            for (const NbtTag& line : lines) {
                static_cast<void>(list.push(line));
            }
            stack.components.set(kLore, NbtTag::ofList(std::move(list)));
        }
        return stack;
    }
    case ComponentEdit::Op::SetBookCover:
        return setBookCover(edit, std::move(stack));
    case ComponentEdit::Op::SetWrittenBookPages:
    case ComponentEdit::Op::SetWritableBookPages:
        return setBookPages(edit, std::move(stack));
    case ComponentEdit::Op::SetFireworkExplosion:
        return setFireworkExplosion(edit, std::move(stack));
    case ComponentEdit::Op::SetFireworks:
        return setFireworks(edit, std::move(stack));
    case ComponentEdit::Op::SetBannerPattern: {
        std::vector<NbtTag> layers =
            edit.append ? elementsOf(stack.components.find(kBannerPatterns)) : std::vector<NbtTag>{};
        for (const NbtTag& layer : elementsOf(edit.compound.find("patterns"))) {
            layers.push_back(layer);
        }
        NbtList list{NbtType::Compound};
        for (NbtTag& layer : layers) {
            static_cast<void>(list.push(std::move(layer)));
        }
        stack.components.set(kBannerPatterns, NbtTag::ofList(std::move(list)));
        return stack;
    }
    // parity: `transmuteCopy`, the count and the patch carried over to the new item.
    case ComponentEdit::Op::SetItem:
        if (stack.count > 0 && edit.id.has_value()) {
            stack.item = *edit.id;
        }
        return stack;
    case ComponentEdit::Op::SetLootTable:
        if (stack.count > 0 && edit.id.has_value()) {
            NbtCompound loot;
            loot.putString("loot_table", edit.id->toString());
            if (edit.seed != 0) {
                loot.putLong("seed", edit.seed);
            }
            stack.components.set(kContainerLoot, NbtTag::ofCompound(std::move(loot)));
        }
        return stack;
    case ComponentEdit::Op::FillPlayerHead:
        if (stack.item == Identifier::vanilla("player_head")) {
            if (const EntityFacts* donor = factsOf(context, edit.entity);
                donor != nullptr && donor->type == Identifier::vanilla("player") && donor->profile.has_value()) {
                stack.components.set(kProfile, NbtTag::ofCompound(*donor->profile));
            }
        }
        return stack;
    case ComponentEdit::Op::SetAttributes:
        return setAttributes(edit, std::move(stack), random, context);
    case ComponentEdit::Op::SetContents:
    case ComponentEdit::Op::ModifyContents:
    default:
        return applyContainerEdit(edit, std::move(stack), random, context);
    }
}

} // namespace basalt::server::world::loot
