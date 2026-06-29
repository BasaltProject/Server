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

enum class GossipType : std::uint8_t {
    kMajorNegative,
    kMinorNegative,
    kMinorPositive,
    kMajorPositive,
    kTrading,
    kCount
};

class Gossips {
public:
    static constexpr std::int32_t kPerEvent = 25;
    static constexpr std::int32_t kPerEverlastingMemory = 20;
    static constexpr std::int32_t kPerTrade = 2;

    void add(const std::string& target, GossipType type, std::int32_t amount);

    [[nodiscard]] std::int32_t reputation(const std::string& target) const;

    void decay();

    std::int32_t transferFrom(const Gossips& source, random::RandomSource& random, std::int32_t maxCount);

    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }

    using Values = std::array<std::int32_t, static_cast<std::size_t>(GossipType::kCount)>;

    [[nodiscard]] const std::map<std::string, Values, std::less<>>& entries() const noexcept { return entries_; }

    /// Writes one value as it stands, with no merging and no limits.
    void set(const std::string& target, GossipType type, std::int32_t value);

    void clear() noexcept { entries_.clear(); }

    [[nodiscard]] wire::nbt::NbtList encode() const;

    void decode(const wire::nbt::NbtList& list);

    [[nodiscard]] static std::string_view name(GossipType type);

    [[nodiscard]] static std::optional<GossipType> byName(std::string_view name);

private:
    std::map<std::string, Values, std::less<>> entries_;
};

} // namespace basalt::server::runtime::entity::villager
