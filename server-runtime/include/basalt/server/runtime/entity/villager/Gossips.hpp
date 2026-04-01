#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::wire::nbt {
class NbtList;
}

namespace basalt::server::runtime::entity::villager {

/// parity: `GossipType`, in the jar's order.
enum class GossipType : std::uint8_t {
    kMajorNegative,
    kMinorNegative,
    kMinorPositive,
    kMajorPositive,
    kTrading,
    kCount
};

/// What villagers say about everybody they have an opinion on.
///
/// parity: `GossipContainer`. Targets are keyed by uuid text so the order a transfer walks them in
/// is fixed; vanilla walks a hash map whose order nobody can reproduce anyway.
class Gossips {
public:
    /// parity: `GossipType.REPUTATION_CHANGE_PER_EVENT` and its siblings.
    static constexpr std::int32_t kPerEvent = 25;
    static constexpr std::int32_t kPerEverlastingMemory = 20;
    static constexpr std::int32_t kPerTrade = 2;

    /// parity: `GossipContainer.add`, capped at the type's maximum and dropped under two.
    void add(const std::string& target, GossipType type, std::int32_t amount);

    /// parity: `getReputation` over every type.
    [[nodiscard]] std::int32_t reputation(const std::string& target) const;

    /// parity: `decay`, one day's worth.
    void decay();

    /// parity: `transferFrom`: up to `maxCount` draws weighted by how much each entry weighs, each
    /// arriving less its per-transfer decay. Answers how many were drawn.
    std::int32_t transferFrom(const Gossips& source, random::RandomSource& random, std::int32_t maxCount);

    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }

    /// parity: `GossipContainer.CODEC`, a list of `{Target, Type, Value}`.
    [[nodiscard]] wire::nbt::NbtList encode() const;

    void decode(const wire::nbt::NbtList& list);

    [[nodiscard]] static std::string_view name(GossipType type);

    [[nodiscard]] static std::optional<GossipType> byName(std::string_view name);

private:
    using Values = std::array<std::int32_t, static_cast<std::size_t>(GossipType::kCount)>;

    std::map<std::string, Values, std::less<>> entries_;
};

} // namespace basalt::server::runtime::entity::villager
