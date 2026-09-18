#pragma once

#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::scoreboard {

/// One objective: a name commands refer to, a criterion, and how the client should draw it.
///
/// The server keeps the criteria that are not `dummy` or `trigger` up to date through
/// `Scoreboard::forAllObjectives`; the per-statistic ones are not offered.
struct Objective {
    std::string name;
    std::string criterion;
    wire::text::Component displayName;
    wire::protocol::play::ObjectiveRenderType renderType = wire::protocol::play::ObjectiveRenderType::Integer;

    /// parity: `numberformat`, which decides how the number on the right is drawn, or not drawn.
    std::optional<wire::protocol::play::NumberFormat> numberFormat;

    /// parity: `displayautoupdate`, off when a sidebar should only change when told to.
    bool displayAutoUpdate = true;

    /// parity: `ObjectiveCriteria.TRIGGER` is the one `/trigger` may touch, by identity.
    [[nodiscard]] bool isTrigger() const { return criterion == "trigger"; }

    /// parity: `ObjectiveCriteria`'s own registrations, in registration order, minus the
    /// per-statistic ones.
    [[nodiscard]] static const std::vector<std::string>& criteria();

    /// parity: `ObjectiveCriteria.isReadOnly`: the six the server keeps up to date itself, which
    /// `/scoreboard players set` refuses to write. `trigger` is not one of them, despite being the
    /// criterion with a lock on it.
    [[nodiscard]] static bool isReadOnly(std::string_view criterion);

    /// parity: `ObjectiveCriteria.getDefaultRenderType`: `health` draws as hearts, the rest as a
    /// number.
    [[nodiscard]] static wire::protocol::play::ObjectiveRenderType defaultRenderType(std::string_view criterion);

    /// parity: `ObjectiveCriteria.byName`: a registered name as it is, a statistic as
    /// `type:value` with `.` for the namespace separator, written back in full. Nothing when neither.
    [[nodiscard]] static std::optional<std::string> canonical(std::string_view criterion);

    /// The statistic key a counter keeps for a statistic criterion (`minecraft:custom/minecraft:jump`
    /// for `minecraft.custom:minecraft.jump`), and the other way round.
    [[nodiscard]] static std::string criterionOfStat(std::string_view statKey);

    /// parity: `TeamColor.getSerializedName`, which names `teamkill.<colour>` and `killedByTeam.<colour>`.
    [[nodiscard]] static std::string_view teamColorName(wire::protocol::play::TeamColor color);
};

/// One holder's score in one objective. parity: `Score`.
///
/// Value, lock and the two per-score overrides live in one object because that is where vanilla
/// keeps them, and it is what makes resetting a score drop the display name, the number format and
/// the trigger's primed state along with the number.
struct Score {
    std::int32_t value = 0;

    /// parity: `Score.locked`, true from birth: `/trigger` needs a `/scoreboard players enable`
    /// first, and locks itself again on use.
    bool locked = true;
    std::optional<wire::text::Component> display;
    std::optional<wire::protocol::play::NumberFormat> numberFormat;
};

/// Objectives, scores and display slots for the whole server.
///
/// Score holders are names, not players: vanilla scores anything with a name, including entities
/// that are not online and "fake players" a datapack invents, and `/scoreboard players set #total x
/// 5` is the ordinary way to keep a counter.
///
/// Holders and objectives are listed in the order they first appeared. Vanilla's own maps are hash
/// maps, so it has no order to match here, only an arbitrary one that changes with the contents.
class Scoreboard {
public:
    [[nodiscard]] std::vector<Objective*> allObjectives();
    [[nodiscard]] std::vector<const Objective*> allObjectives() const;

    [[nodiscard]] Objective* get(std::string_view name);
    [[nodiscard]] const Objective* get(std::string_view name) const;

    /// Answers whether it went in, which it does not when the name is taken.
    bool add(Objective objective);

    /// parity: `removeObjective`, which drops the objective's scores and clears any slot showing
    /// it. A holder left with no scores at all stays on the books, which is what vanilla does here
    /// and deliberately not what it does when a score is reset.
    std::unique_ptr<Objective> remove(std::string_view name);

    [[nodiscard]] const std::string* displayed(wire::protocol::play::DisplaySlot slot) const;

    void display(wire::protocol::play::DisplaySlot slot, std::optional<std::string> objectiveName);

    [[nodiscard]] std::optional<std::int32_t> score(std::string_view holder, std::string_view objectiveName) const;

    void setScore(std::string_view holder, std::string_view objectiveName, std::int32_t value);

    /// Answers the new value, saturating rather than wrapping at the ends of the range.
    std::int32_t addScore(std::string_view holder, std::string_view objectiveName, std::int32_t delta);

    /// parity: `resetSinglePlayerScore` and `resetAllPlayerScores`. A holder whose last score goes
    /// stops being tracked, which is what makes `/trigger` unprimed again after a reset.
    void resetScore(std::string_view holder, std::optional<std::string_view> objectiveName);

    /// Every holder with a score in `objectiveName`, in the order they first appeared, which is
    /// what `/scoreboard players list` prints.
    [[nodiscard]] std::vector<std::string> holdersIn(std::string_view objectiveName) const;

    [[nodiscard]] std::vector<std::string> holders() const;

    [[nodiscard]] std::vector<std::pair<std::string, std::int32_t>> scoresOf(std::string_view holder) const;

    /// Per-holder overrides `/scoreboard players display` sets, which live on the score itself.
    [[nodiscard]] const wire::text::Component* displayNameOf(std::string_view holder,
                                                             std::string_view objectiveName) const;

    void setDisplayName(std::string_view holder, std::string_view objectiveName,
                        std::optional<wire::text::Component> name);

    [[nodiscard]] const wire::protocol::play::NumberFormat* numberFormatOf(std::string_view holder,
                                                                           std::string_view objectiveName) const;

    void setNumberFormat(std::string_view holder, std::string_view objectiveName,
                         std::optional<wire::protocol::play::NumberFormat> format);

    /// parity: `forAllObjectives`: every objective of this criterion gets `update` applied to the
    /// holder's score, created at zero when missing. Answers each objective touched and its new value.
    std::vector<std::pair<std::string, std::int32_t>>
    forAllObjectives(std::string_view criterion, std::string_view holder,
                     const std::function<std::int32_t(std::int32_t)>& update);

    /// parity: `/scoreboard players enable`, which creates the score at zero and unlocks it.
    void enableTrigger(std::string_view holder, std::string_view objectiveName);

    [[nodiscard]] bool isTriggerEnabled(std::string_view holder, std::string_view objectiveName) const;

    /// parity: `Score.lock`, which `/trigger` calls on itself so one enable buys one use.
    void disableTrigger(std::string_view holder, std::string_view objectiveName);

private:
    struct Holder {
        std::string name;
        std::vector<std::pair<std::string, Score>> scores;
    };

    [[nodiscard]] const Score* find(std::string_view holder, std::string_view objectiveName) const;
    [[nodiscard]] Score* find(std::string_view holder, std::string_view objectiveName);
    [[nodiscard]] Score& create(std::string_view holder, std::string_view objectiveName);

    std::vector<std::unique_ptr<Objective>> objectives_;
    std::vector<Holder> holders_;
    std::vector<std::pair<wire::protocol::play::DisplaySlot, std::string>> displaySlots_;
};

} // namespace basalt::server::runtime::scoreboard
