#include <basalt/server/runtime/world/Enchanting.hpp>
#include <basalt/server/runtime/world/ItemEnchantability.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace basalt::server::runtime::world::enchanting {
namespace {

using wire::common::Identifier;

/// What the table is picking between: an enchantment, the level it would come at, and how often it
/// is drawn against the others.
struct Candidate {
    Identifier enchantment;
    std::int32_t level = 1;
    std::int32_t weight = 0;
};

/// parity: `WeightedRandom.getRandomItem`, which walks the list subtracting weights.
[[nodiscard]] Candidate weightedPick(random::RandomSource& random, const std::vector<Candidate>& from) {
    std::int32_t total = 0;
    for (const Candidate& candidate : from) {
        total += candidate.weight;
    }
    std::int32_t index = random.nextInt(std::max(total, 1));
    for (const Candidate& candidate : from) {
        index -= candidate.weight;
        if (index < 0) {
            return candidate;
        }
    }
    return from.back();
}

/// parity: `getAvailableEnchantmentResults`: the highest level of each whose band the cost falls
/// in, and a plain book takes anything.
[[nodiscard]] std::vector<Candidate> available(const EnchantmentBooks& books, const storage::StoredItemStack& stack,
                                               const std::int32_t cost, const std::string_view tag) {
    const bool book = stack.id == "minecraft:book";
    std::vector<Candidate> candidates;
    for (const Identifier& id : books.inTag(tag)) {
        if (!book && !books.isPrimaryFor(id, stack)) {
            continue;
        }
        for (std::int32_t level = books.maxLevel(id); level >= 1; --level) {
            if (cost >= books.minCost(id, level) && cost <= books.maxCost(id, level)) {
                candidates.push_back({.enchantment = id, .level = level, .weight = books.weight(id)});
                break;
            }
        }
    }
    return candidates;
}

} // namespace

std::span<const std::array<std::int32_t, 3>> bookshelfOffsets() {
    static const std::vector<std::array<std::int32_t, 3>> kOffsets = [] {
        std::vector<std::array<std::int32_t, 3>> made;
        for (std::int32_t x = -2; x <= 2; ++x) {
            for (std::int32_t y = 0; y <= 1; ++y) {
                for (std::int32_t z = -2; z <= 2; ++z) {
                    if (std::abs(x) == 2 || std::abs(z) == 2) {
                        made.push_back({x, y, z});
                    }
                }
            }
        }
        return made;
    }();
    return kOffsets;
}

std::int32_t costFor(random::RandomSource& random, const std::int32_t slot, std::int32_t bookshelves) {
    /// parity: `getEnchantmentCost`, which stops counting shelves at fifteen.
    constexpr std::int32_t kMaxBookshelves = 15;

    bookshelves = std::clamp(bookshelves, 0, kMaxBookshelves);
    const std::int32_t drawn = random.nextInt(8) + 1 + (bookshelves >> 1) + random.nextInt(bookshelves + 1);
    if (slot == 0) {
        return std::max(drawn / 3, 1);
    }
    return slot == 1 ? ((drawn * 2 / 3) + 1) : std::max(drawn, bookshelves * 2);
}

std::vector<Offer> select(random::RandomSource& random, const EnchantmentBooks& books,
                          const storage::StoredItemStack& stack, std::int32_t cost, const std::string_view tag) {
    /// parity: the roll that keeps adding enchantments while it comes up under the cost.
    constexpr std::int32_t kMoreBound = 50;
    /// parity: `selectEnchantment`'s own fifteen hundredths of spread.
    constexpr float kSpan = 0.15F;

    std::vector<Offer> offers;
    const std::optional<std::int32_t> enchantability = itemEnchantability::valueOf(stack.id);
    if (!enchantability.has_value()) {
        return offers;
    }
    cost += 1 + random.nextInt(*enchantability / 4 + 1) + random.nextInt(*enchantability / 4 + 1);
    const float spread = (random.nextFloat() + random.nextFloat() - 1.0F) * kSpan;
    cost = std::max(
        1, static_cast<std::int32_t>(std::lround(static_cast<float>(cost) + (static_cast<float>(cost) * spread))));

    std::vector<Candidate> candidates = available(books, stack, cost, tag);
    if (candidates.empty()) {
        return offers;
    }
    Candidate picked = weightedPick(random, candidates);
    offers.push_back({.enchantment = picked.enchantment, .level = picked.level});
    while (random.nextInt(kMoreBound) <= cost) {
        const Identifier last = offers.back().enchantment;
        std::erase_if(candidates,
                      [&books, &last](const Candidate& one) { return !books.compatible(last, one.enchantment); });
        if (candidates.empty()) {
            break;
        }
        picked = weightedPick(random, candidates);
        offers.push_back({.enchantment = picked.enchantment, .level = picked.level});
        cost /= 2;
    }
    return offers;
}

} // namespace basalt::server::runtime::world::enchanting
