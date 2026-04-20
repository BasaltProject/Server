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

struct Objective {
    std::string name;
    std::string criterion;
    wire::text::Component displayName;
    wire::protocol::play::ObjectiveRenderType renderType = wire::protocol::play::ObjectiveRenderType::Integer;

    /// parity: `numberformat`.
    std::optional<wire::protocol::play::NumberFormat> numberFormat;

    /// parity: `displayautoupdate`, off when a sidebar should only change when told to.
    bool displayAutoUpdate = true;

    [[nodiscard]] bool isTrigger() const { return criterion == "trigger"; }

    [[nodiscard]] static const std::vector<std::string>& criteria();

    [[nodiscard]] static bool isReadOnly(std::string_view criterion);

    [[nodiscard]] static wire::protocol::play::ObjectiveRenderType defaultRenderType(std::string_view criterion);

    [[nodiscard]] static std::optional<std::string> canonical(std::string_view criterion);

    [[nodiscard]] static std::string criterionOfStat(std::string_view statKey);

    [[nodiscard]] static std::string_view teamColorName(wire::protocol::play::TeamColor color);
};

struct Score {
    std::int32_t value = 0;

    bool locked = true;
    std::optional<wire::text::Component> display;
    std::optional<wire::protocol::play::NumberFormat> numberFormat;
};

class Scoreboard {
public:
    [[nodiscard]] std::vector<Objective*> allObjectives();
    [[nodiscard]] std::vector<const Objective*> allObjectives() const;

    [[nodiscard]] Objective* get(std::string_view name);
    [[nodiscard]] const Objective* get(std::string_view name) const;

    bool add(Objective objective);

    std::unique_ptr<Objective> remove(std::string_view name);

    [[nodiscard]] const std::string* displayed(wire::protocol::play::DisplaySlot slot) const;

    void display(wire::protocol::play::DisplaySlot slot, std::optional<std::string> objectiveName);

    [[nodiscard]] std::optional<std::int32_t> score(std::string_view holder, std::string_view objectiveName) const;

    void setScore(std::string_view holder, std::string_view objectiveName, std::int32_t value);

    std::int32_t addScore(std::string_view holder, std::string_view objectiveName, std::int32_t delta);

    void resetScore(std::string_view holder, std::optional<std::string_view> objectiveName);

    [[nodiscard]] std::vector<std::string> holdersIn(std::string_view objectiveName) const;

    [[nodiscard]] std::vector<std::string> holders() const;

    [[nodiscard]] std::vector<std::pair<std::string, std::int32_t>> scoresOf(std::string_view holder) const;

    [[nodiscard]] const wire::text::Component* displayNameOf(std::string_view holder,
                                                             std::string_view objectiveName) const;

    void setDisplayName(std::string_view holder, std::string_view objectiveName,
                        std::optional<wire::text::Component> name);

    [[nodiscard]] const wire::protocol::play::NumberFormat* numberFormatOf(std::string_view holder,
                                                                           std::string_view objectiveName) const;

    void setNumberFormat(std::string_view holder, std::string_view objectiveName,
                         std::optional<wire::protocol::play::NumberFormat> format);

    std::vector<std::pair<std::string, std::int32_t>>
    forAllObjectives(std::string_view criterion, std::string_view holder,
                     const std::function<std::int32_t(std::int32_t)>& update);

    void enableTrigger(std::string_view holder, std::string_view objectiveName);

    [[nodiscard]] bool isTriggerEnabled(std::string_view holder, std::string_view objectiveName) const;

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
