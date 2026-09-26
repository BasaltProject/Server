#include "LootComponentEdit.hpp"

#include <basalt/server/registry/Items.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <numeric>
#include <utility>

namespace basalt::server::world::loot {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr std::string_view kContainer = "minecraft:container";
constexpr std::string_view kBundleContents = "minecraft:bundle_contents";

[[nodiscard]] std::int32_t maxStackOf(const ItemStack& stack) {
    if (const NbtTag* size = stack.components.find("minecraft:max_stack_size"); size != nullptr) {
        if (const auto* value = size->get<std::int32_t>(); value != nullptr) {
            return *value;
        }
    }
    return std::max(server::registry::itemStacking::maxStackSize(stack.item), 1);
}

[[nodiscard]] NbtCompound stackToNbt(const ItemStack& stack) {
    NbtCompound out;
    out.putString("id", stack.item.toString());
    if (stack.count != 1) {
        out.putInt("count", stack.count);
    }
    if (!stack.components.names().empty()) {
        out.set("components", NbtTag::ofCompound(stack.components));
    }
    return out;
}

[[nodiscard]] ItemStack stackFromNbt(const NbtCompound& nbt) {
    ItemStack stack{.item = Identifier::vanilla("air"), .count = 0};
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(nbt.getString("id"));
    if (!id.has_value()) {
        return stack;
    }
    stack.item = *id;
    stack.count = nbt.find("count") == nullptr ? 1 : nbt.getInt("count");
    if (const NbtCompound* components = nbt.getCompound("components"); components != nullptr) {
        stack.components = *components;
    }
    return stack;
}

[[nodiscard]] bool isEmpty(const ItemStack& stack) {
    return stack.count <= 0 || stack.item == Identifier::vanilla("air");
}

/// A `Fraction` in lowest terms, which is what bundle weights are counted in.
struct Fraction {
    std::int64_t numerator = 0;
    std::int64_t denominator = 1;

    [[nodiscard]] static Fraction of(std::int64_t numerator, std::int64_t denominator) {
        const std::int64_t divisor = std::gcd(numerator, denominator);
        return divisor == 0 ? Fraction{} : Fraction{numerator / divisor, denominator / divisor};
    }

    [[nodiscard]] Fraction plus(const Fraction& other) const {
        return of((numerator * other.denominator) + (other.numerator * denominator), denominator * other.denominator);
    }

    [[nodiscard]] Fraction times(std::int64_t count) const { return of(numerator * count, denominator); }
};

/// parity: `BundleContents.BUNDLE_IN_BUNDLE_WEIGHT`.
constexpr Fraction kBundleInBundle{1, 16};

[[nodiscard]] std::vector<ItemStack> listItems(const NbtTag* tag) {
    std::vector<ItemStack> items;
    const NbtList* list = tag == nullptr ? nullptr : tag->get<NbtList>();
    if (list == nullptr) {
        return items;
    }
    for (const NbtTag& element : *list) {
        if (const NbtCompound* stack = element.get<NbtCompound>(); stack != nullptr) {
            items.push_back(stackFromNbt(*stack));
        }
    }
    return items;
}

[[nodiscard]] Fraction weightOf(const ItemStack& stack);

/// parity: `BundleContents.computeContentWeight`.
[[nodiscard]] Fraction contentWeight(const std::vector<ItemStack>& items) {
    Fraction total;
    for (const ItemStack& item : items) {
        total = total.plus(weightOf(item).times(item.count));
    }
    return total;
}

/// parity: `BundleContents.getWeight`.
[[nodiscard]] Fraction weightOf(const ItemStack& stack) {
    if (const NbtTag* nested = stack.components.find(kBundleContents); nested != nullptr) {
        return contentWeight(listItems(nested)).plus(kBundleInBundle);
    }
    if (const NbtTag* bees = stack.components.find("minecraft:bees"); bees != nullptr) {
        const NbtList* list = bees->get<NbtList>();
        if (list != nullptr && !list->empty()) {
            return Fraction{1, 1};
        }
    }
    return Fraction::of(1, maxStackOf(stack));
}

/// parity: `ItemStack.isStackable`.
[[nodiscard]] bool stackable(const ItemStack& stack) {
    const bool damaged =
        server::registry::itemDurability::isDamageable(stack.item) && stack.components.getInt("minecraft:damage") > 0;
    return maxStackOf(stack) > 1 && !damaged;
}

/// parity: `BundleContents.Mutable.tryInsert`, each stack put in front and merged with its twin.
void tryInsert(std::vector<ItemStack>& items, Fraction& weight, ItemStack stack) {
    // parity: `Item.canFitInsideContainerItems`, which only a shulker box answers no to.
    if (isEmpty(stack) || stack.item.path().ends_with("shulker_box")) {
        return;
    }
    const Fraction itemWeight = weightOf(stack);
    const Fraction remaining = Fraction::of((weight.denominator - weight.numerator), weight.denominator);
    // parity: `Fraction.divideBy(...).intValue()`, truncated, never below zero.
    const std::int64_t fits = std::max<std::int64_t>(
        (remaining.numerator * itemWeight.denominator) / (remaining.denominator * itemWeight.numerator), 0);
    const auto amount = static_cast<std::int32_t>(std::min<std::int64_t>(stack.count, fits));
    if (amount == 0) {
        return;
    }
    weight = weight.plus(itemWeight.times(amount));
    const auto twin = !stackable(stack) ? items.end() : std::ranges::find_if(items, [&stack](const ItemStack& held) {
        return held.item == stack.item && held.components == stack.components;
    });
    ItemStack placed = stack;
    if (twin != items.end()) {
        placed = *twin;
        placed.count += amount;
        items.erase(twin);
    } else {
        placed.count = amount;
    }
    items.insert(items.begin(), std::move(placed));
}

/// parity: `ContainerComponent.copyWithContents` for the three components.
void setContents(ItemStack& holder, std::string_view component, const std::vector<ItemStack>& contents) {
    if (component == kContainer) {
        // parity: `ItemContainerContents.fromItems`, each stack at its position up to the last one.
        NbtList slots{NbtType::Compound};
        for (std::size_t index = 0; index < contents.size(); ++index) {
            if (isEmpty(contents[index])) {
                continue;
            }
            NbtCompound slot;
            slot.putInt("slot", static_cast<std::int32_t>(index));
            slot.set("item", NbtTag::ofCompound(stackToNbt(contents[index])));
            static_cast<void>(slots.push(NbtTag::ofCompound(std::move(slot))));
        }
        holder.components.set(component, NbtTag::ofList(std::move(slots)));
        return;
    }
    std::vector<ItemStack> kept;
    if (component == kBundleContents) {
        Fraction weight;
        for (const ItemStack& stack : contents) {
            tryInsert(kept, weight, stack);
        }
    } else {
        std::ranges::copy_if(contents, std::back_inserter(kept),
                             [](const ItemStack& stack) { return !isEmpty(stack); });
    }
    NbtList list{NbtType::Compound};
    for (const ItemStack& stack : kept) {
        static_cast<void>(list.push(NbtTag::ofCompound(stackToNbt(stack))));
    }
    holder.components.set(component, NbtTag::ofList(std::move(list)));
}

[[nodiscard]] bool numbered(std::string_view name, std::string_view prefix, std::int32_t size) {
    if (!name.starts_with(prefix)) {
        return false;
    }
    const std::string_view rest = name.substr(prefix.size());
    if (rest == "*") {
        return true;
    }
    std::int32_t index = -1;
    const auto [end, error] = std::from_chars(rest.data(), rest.data() + rest.size(), index);
    return error == std::errc{} && end == rest.data() + rest.size() && index >= 0 && index < size &&
           std::to_string(index) == rest;
}

} // namespace

std::vector<ItemStack> containerItems(const ItemStack& holder, std::string_view component) {
    const NbtTag* held = holder.components.find(component);
    if (component != kContainer) {
        return listItems(held);
    }
    // parity: `ItemContainerContents.itemCopies`, the empty slots between included.
    std::vector<ItemStack> items;
    const NbtList* slots = held == nullptr ? nullptr : held->get<NbtList>();
    if (slots == nullptr) {
        return items;
    }
    for (const NbtTag& element : *slots) {
        const NbtCompound* slot = element.get<NbtCompound>();
        const NbtCompound* item = slot == nullptr ? nullptr : slot->getCompound("item");
        if (item == nullptr) {
            continue;
        }
        const auto index = static_cast<std::size_t>(std::clamp(slot->getInt("slot"), 0, 255));
        while (items.size() <= index) {
            items.push_back(ItemStack{.item = Identifier::vanilla("air"), .count = 0});
        }
        items[index] = stackFromNbt(*item);
    }
    return items;
}

ItemStack applyContainerEdit(const ComponentEdit& edit, ItemStack stack, random::RandomSource& random,
                             const LootContext& context) {
    if (isEmpty(stack)) {
        return stack;
    }
    if (edit.op == ComponentEdit::Op::SetContents) {
        std::vector<ItemStack> contents;
        rollContainerEntries(edit.entries, random, context,
                             [&contents](ItemStack made) { contents.push_back(std::move(made)); });
        setContents(stack, edit.component, contents);
        return stack;
    }
    // parity: `ContainerComponentManipulator.modifyItems`: nothing without the component, and each
    // stack run through the modifier and cut to its stack size.
    if (stack.components.find(edit.component) == nullptr) {
        return stack;
    }
    std::vector<ItemStack> items = containerItems(stack, edit.component);
    for (ItemStack& item : items) {
        if (isEmpty(item)) {
            continue;
        }
        for (const LootFunction& function : edit.modifier) {
            item = function.apply(std::move(item), random, std::nullopt, context);
        }
        item.count = std::min(item.count, maxStackOf(item));
    }
    setContents(stack, edit.component, items);
    return stack;
}

std::vector<ItemStack> SlotSource::provide(const LootContext& context) const {
    std::vector<ItemStack> slotsFound;
    switch (kind) {
    case Kind::Group:
        for (const SlotSource& term : terms) {
            std::vector<ItemStack> part = term.provide(context);
            slotsFound.insert(slotsFound.end(), part.begin(), part.end());
        }
        return slotsFound;
    case Kind::Range:
        return context.slotsOf ? context.slotsOf(source, slots) : slotsFound;
    case Kind::Contents:
        for (const ItemStack& holder : child->provide(context)) {
            std::vector<ItemStack> part = containerItems(holder, component);
            slotsFound.insert(slotsFound.end(), part.begin(), part.end());
        }
        return slotsFound;
    case Kind::Filtered:
        for (ItemStack& stack : child->provide(context)) {
            if (filter.test(stack.item, stack.components, context.tags, stack.count)) {
                slotsFound.push_back(std::move(stack));
            }
        }
        return slotsFound;
    case Kind::Limit:
        slotsFound = child->provide(context);
        if (static_cast<std::int32_t>(slotsFound.size()) > limit) {
            slotsFound.erase(slotsFound.begin() + limit, slotsFound.end());
        }
        return slotsFound;
    case Kind::Empty:
    default:
        return slotsFound;
    }
}

bool isSlotRange(std::string_view name) {
    static constexpr std::array<std::string_view, 15> kSingle{
        "contents",   "weapon",      "weapon.mainhand", "weapon.offhand", "weapon.*",
        "armor.head", "armor.chest", "armor.legs",      "armor.feet",     "armor.body",
        "armor.*",    "saddle",      "horse.chest",     "player.cursor",  ""};
    if (!name.empty() && std::ranges::find(kSingle, name) != kSingle.end()) {
        return true;
    }
    return numbered(name, "container.", 54) || numbered(name, "hotbar.", 9) || numbered(name, "inventory.", 27) ||
           numbered(name, "enderchest.", 27) || numbered(name, "mob.inventory.", 8) || numbered(name, "horse.", 15) ||
           numbered(name, "player.crafting.", 4);
}

} // namespace basalt::server::world::loot
