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

/// A `min..max` bound as a selector writes it. An absent end does not bound that side.
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

/// A live entity as NBT, in the shape vanilla's own entity data uses for the same fields.
///
/// One view for `/data get entity` and for a selector's `nbt=`, because a player who reads a field
/// out of the first and filters on it with the second has to see the same name and the same type.
/// Only what this build models: everything else arrives with the system that makes it exist.
namespace entityNbtView {

[[nodiscard]] wire::nbt::NbtCompound of(const player::PlayerSession& player);

[[nodiscard]] wire::nbt::NbtCompound of(const entity::ServerEntity& entity);

/// What `nbt=` compares against for whatever a selector points at.
[[nodiscard]] wire::nbt::NbtCompound of(const CommandTarget& target);

} // namespace entityNbtView

/// A parsed `@a[...]`, `@e[...]`, a plain player name or a UUID: what to look for and how many.
///
/// parity: `EntitySelector` plus `EntitySelectorParser`, split the same way because the split is
/// observable: an unknown option is a syntax error while parsing, not an empty result later.
///
/// Two answers differ from vanilla for want of data, not by choice: `dx/dy/dz` tests a real box for
/// players and a point for anything with no dimensions, and `nbt=` compares against the partial
/// entity view above.
class EntitySelector {
public:
    /// One filter the selector applies to a candidate.
    using Predicate = std::function<bool(const CommandTarget& target)>;

    /// Everything the parser worked out. A struct because there are seventeen of them and the
    /// parser fills them one option at a time.
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

    /// Parses one selector, a player name or a UUID. What it may match is decided later, by
    /// `select`.
    [[nodiscard]] static Parsed<EntitySelector> parse(StringReader& reader, const CommandSource& source,
                                                      const CommandEnvironment& environment);

    /// Everything this selector matches right now.
    ///
    /// `playersOnly` is the argument type's own restriction (`minecraft:entity` with `playersOnly`),
    /// not the selector's: `@e` handed to `/kick` matches players and nothing else, exactly as
    /// vanilla's `findPlayers` does with the same selector.
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
