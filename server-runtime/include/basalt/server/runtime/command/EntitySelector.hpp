#pragma once

#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/StringReader.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::command {

template <typename T>
struct SelectorRange {
    std::optional<T> min;
    std::optional<T> max;

    [[nodiscard]] bool matches(T value) const {
        return (!min.has_value() || value >= *min) && (!max.has_value() || value <= *max);
    }

    friend bool operator==(const SelectorRange&, const SelectorRange&) = default;
};

/// parity: the four `sort=` values, which is also what each selector letter starts with.
enum class SelectorOrder : std::uint8_t { Arbitrary, Nearest, Furthest, Random };

namespace entityNbtView {

[[nodiscard]] wire::nbt::NbtCompound of(const player::PlayerSession& player);

[[nodiscard]] wire::nbt::NbtCompound of(const entity::ServerEntity& entity);

[[nodiscard]] wire::nbt::NbtCompound of(const CommandTarget& target);

} // namespace entityNbtView

/// parity: `EntitySelector`.
class EntitySelector {
public:
    using Predicate = std::function<bool(const CommandTarget& target)>;

    struct Parts {
        std::int32_t maxResults = 0;
        bool includesEntities = false;
        bool worldLimited = false;
        std::vector<Predicate> predicates;
        std::optional<SelectorRange<double>> distance;
        std::optional<double> anchorX;
        std::optional<double> anchorY;
        std::optional<double> anchorZ;
        std::optional<double> deltaX;
        std::optional<double> deltaY;
        std::optional<double> deltaZ;
        SelectorOrder order = SelectorOrder::Arbitrary;
        bool currentEntity = false;
        std::optional<std::string> playerName;
        std::optional<std::string> entityUuid;
        std::optional<wire::common::Identifier> typeLimit;
    };

    explicit EntitySelector(Parts parts);

    [[nodiscard]] static Parsed<EntitySelector> parse(StringReader& reader, const CommandSource& source,
                                                      const CommandEnvironment& environment);

    [[nodiscard]] std::vector<CommandTarget> select(const CommandSource& source, const CommandEnvironment& environment,
                                                    bool playersOnly) const;

    [[nodiscard]] std::int32_t maxResults() const noexcept { return parts_.maxResults; }

private:
    [[nodiscard]] bool matches(const CommandTarget& target, double x, double y, double z) const;

    [[nodiscard]] bool withinBox(const CommandTarget& target, double x, double y, double z) const;

    [[nodiscard]] std::vector<CommandTarget> sortAndLimit(std::vector<CommandTarget> matched, double x, double y,
                                                          double z) const;

    Parts parts_;
};

} // namespace basalt::server::runtime::command
