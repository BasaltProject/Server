#include <basalt/server/runtime/entity/villager/Gossips.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace basalt::server::runtime::entity::villager {
namespace {

struct TypeFacts {
    std::string_view name;
    std::int32_t weight;
    std::int32_t max;
    std::int32_t decayPerDay;
    std::int32_t decayPerTransfer;
};

/// parity: `GossipType`'s own table.
constexpr std::array<TypeFacts, static_cast<std::size_t>(GossipType::kCount)> kFacts{{
    {"major_negative", -5, 100, 10, 10},
    {"minor_negative", -1, 200, 20, 20},
    {"minor_positive", 1, 25, 1, 5},
    {"major_positive", 5, 20, 0, 20},
    {"trading", 1, 25, 2, 20},
}};

/// parity: `GossipContainer.DISCARD_THRESHOLD`.
constexpr std::int32_t kDiscardThreshold = 2;

[[nodiscard]] const TypeFacts& factsOf(std::size_t type) { return kFacts[type]; }

struct Entry {
    const std::string* target;
    std::size_t type;
    std::int32_t value;
};

} // namespace

void Gossips::set(const std::string& target, GossipType type, std::int32_t value) {
    Values& values = entries_[target];
    values[static_cast<std::size_t>(type)] = value;
    if (std::ranges::all_of(values, [](std::int32_t each) { return each == 0; })) {
        entries_.erase(target);
    }
}

void Gossips::add(const std::string& target, GossipType type, std::int32_t amount) {
    const auto index = static_cast<std::size_t>(type);
    Values& values = entries_[target];
    const TypeFacts& facts = factsOf(index);
    const std::int32_t old = values[index];
    // parity: `mergeValuesForAddition`, where an absent entry merges as the amount itself.
    std::int32_t merged = amount;
    if (old != 0) {
        const std::int32_t sum = old + amount;
        merged = sum > facts.max ? std::max(facts.max, old) : sum;
    }
    // parity: `makeSureValueIsntTooLowOrTooHigh`.
    merged = std::min(merged, facts.max);
    values[index] = merged < kDiscardThreshold ? 0 : merged;
    if (std::ranges::all_of(values, [](std::int32_t value) { return value == 0; })) {
        entries_.erase(target);
    }
}

std::int32_t Gossips::reputation(const std::string& target) const {
    const auto found = entries_.find(target);
    if (found == entries_.end()) {
        return 0;
    }
    std::int32_t total = 0;
    for (std::size_t index = 0; index < found->second.size(); ++index) {
        total += found->second[index] * factsOf(index).weight;
    }
    return total;
}

void Gossips::decay() {
    for (auto it = entries_.begin(); it != entries_.end();) {
        for (std::size_t index = 0; index < it->second.size(); ++index) {
            std::int32_t& value = it->second[index];
            if (value == 0) {
                continue;
            }
            const std::int32_t decayed = value - factsOf(index).decayPerDay;
            value = decayed < kDiscardThreshold ? 0 : decayed;
        }
        if (std::ranges::all_of(it->second, [](std::int32_t value) { return value == 0; })) {
            it = entries_.erase(it);
        } else {
            ++it;
        }
    }
}

std::int32_t Gossips::transferFrom(const Gossips& source, random::RandomSource& random, std::int32_t maxCount) {
    std::vector<Entry> entries;
    for (const auto& [target, values] : source.entries_) {
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (values[index] != 0) {
                entries.push_back(Entry{.target = &target, .type = index, .value = values[index]});
            }
        }
    }
    if (entries.empty()) {
        return 0;
    }
    // parity: `selectGossipsForTransfer`, a weighted draw into an identity set.
    std::vector<std::int32_t> ranges(entries.size());
    std::int32_t end = 0;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        end += std::abs(entries[index].value * factsOf(entries[index].type).weight);
        ranges[index] = end - 1;
    }
    std::vector<bool> chosen(entries.size(), false);
    std::int32_t drawn = 0;
    for (std::int32_t draw = 0; draw < maxCount; ++draw) {
        const std::int32_t choice = random.nextInt(end);
        const auto at = std::ranges::lower_bound(ranges, choice);
        const auto index = static_cast<std::size_t>(at - ranges.begin());
        if (!chosen[index]) {
            chosen[index] = true;
            ++drawn;
        }
    }
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (!chosen[index]) {
            continue;
        }
        const Entry& entry = entries[index];
        const std::int32_t decayed = entry.value - factsOf(entry.type).decayPerTransfer;
        if (decayed >= kDiscardThreshold) {
            std::int32_t& value = entries_[*entry.target][entry.type];
            value = std::max(value, decayed);
        }
    }
    return drawn;
}

wire::nbt::NbtList Gossips::encode() const {
    wire::nbt::NbtList list{wire::nbt::NbtType::Compound};
    for (const auto& [target, values] : entries_) {
        const std::optional<wire::common::Uuid> uuid = wire::common::Uuid::parse(target);
        if (!uuid.has_value()) {
            continue;
        }
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (values[index] <= 0) {
                continue;
            }
            wire::nbt::NbtCompound entry;
            entry.putIntArray("Target", {static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid->high() >> 32)),
                                         static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid->high())),
                                         static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid->low() >> 32)),
                                         static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid->low()))});
            entry.putString("Type", std::string{factsOf(index).name});
            entry.putInt("Value", values[index]);
            static_cast<void>(list.push(wire::nbt::NbtTag::ofCompound(std::move(entry))));
        }
    }
    return list;
}

void Gossips::decode(const wire::nbt::NbtList& list) {
    entries_.clear();
    for (const wire::nbt::NbtTag& tag : list.elements()) {
        const wire::nbt::NbtCompound* entry = tag.get<wire::nbt::NbtCompound>();
        if (entry == nullptr) {
            continue;
        }
        const std::span<const std::int32_t> parts = entry->getIntArray("Target");
        const std::optional<GossipType> type = byName(entry->getString("Type"));
        const std::int32_t value = entry->getInt("Value");
        if (parts.size() != 4 || !type.has_value() || value <= 0) {
            continue;
        }
        const auto word = [parts](std::size_t at) {
            return static_cast<std::uint64_t>(static_cast<std::uint32_t>(parts[at]));
        };
        const wire::common::Uuid uuid{(word(0) << 32) | word(1), (word(2) << 32) | word(3)};
        entries_[uuid.toString()][static_cast<std::size_t>(*type)] = value;
    }
}

std::string_view Gossips::name(GossipType type) { return factsOf(static_cast<std::size_t>(type)).name; }

std::optional<GossipType> Gossips::byName(std::string_view name) {
    for (std::size_t index = 0; index < kFacts.size(); ++index) {
        if (kFacts[index].name == name) {
            return static_cast<GossipType>(index);
        }
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::entity::villager
