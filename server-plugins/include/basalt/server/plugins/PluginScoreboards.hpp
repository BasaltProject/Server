#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::plugins {

/// Text is a JSON text component throughout.
struct PluginObjective {
    std::string name;
    std::string criterion;
    std::string displayName;
    bool hearts = false;
    bool autoUpdate = true;
    /// "", "blank", "fixed" or "styled"; the text is the fixed value, or a component carrying the style.
    std::string numberFormat;
    std::string numberFormatText;
    bool readOnly = false;
};

struct PluginScoreDisplay {
    std::optional<std::string> customName;
    std::string numberFormat;
    std::string numberFormatText;
};

struct PluginTeam {
    std::string name;
    std::string displayName;
    std::string prefix;
    std::string suffix;
    /// A colour name such as "red", or empty for none.
    std::string color;
    bool friendlyFire = true;
    bool seeFriendlyInvisibles = true;
    /// "always", "never", "hideForOtherTeams" or "hideForOwnTeam".
    std::string nameTagVisibility = "always";
    std::string deathMessageVisibility = "always";
    /// "always", "never", "pushOtherTeams" or "pushOwnTeam".
    std::string collisionRule = "always";
    std::vector<std::string> members;
};

/// The server's own scoreboard (handle 0) and the boards a plugin builds, each drawn for the players it is shown to.
class PluginScoreboards {
public:
    PluginScoreboards() = default;
    PluginScoreboards(const PluginScoreboards&) = delete;
    PluginScoreboards& operator=(const PluginScoreboards&) = delete;
    PluginScoreboards(PluginScoreboards&&) = delete;
    PluginScoreboards& operator=(PluginScoreboards&&) = delete;
    virtual ~PluginScoreboards() = default;

    [[nodiscard]] static PluginScoreboards& unavailable();

    static constexpr std::int32_t kMain = 0;

    /// A new empty board; -1 when scoreboards are unavailable.
    virtual std::int32_t create() { return -1; }

    /// Forgets a board made by `create` once nothing shows it and no plugin holds it.
    virtual void release(std::int32_t board) { static_cast<void>(board); }

    [[nodiscard]] virtual std::vector<PluginObjective> objectives(std::int32_t board) {
        static_cast<void>(board);
        return {};
    }

    [[nodiscard]] virtual std::optional<PluginObjective> objective(std::int32_t board, std::string_view name) {
        static_cast<void>(board);
        static_cast<void>(name);
        return std::nullopt;
    }

    /// False when the name is taken or the criterion unknown.
    virtual bool addObjective(std::int32_t board, const PluginObjective& objective) {
        static_cast<void>(board);
        static_cast<void>(objective);
        return false;
    }

    /// Changes the display name, render type, number format and auto update of an existing objective.
    virtual bool updateObjective(std::int32_t board, const PluginObjective& objective) {
        static_cast<void>(board);
        static_cast<void>(objective);
        return false;
    }

    virtual bool removeObjective(std::int32_t board, std::string_view name) {
        static_cast<void>(board);
        static_cast<void>(name);
        return false;
    }

    /// Slots are the vanilla ids: 0 list, 1 sidebar, 2 below name, 3 to 18 the sidebar of one team colour.
    [[nodiscard]] virtual std::optional<std::string> displayed(std::int32_t board, std::int32_t slot) {
        static_cast<void>(board);
        static_cast<void>(slot);
        return std::nullopt;
    }

    virtual void display(std::int32_t board, std::int32_t slot, const std::optional<std::string>& objective) {
        static_cast<void>(board);
        static_cast<void>(slot);
        static_cast<void>(objective);
    }

    [[nodiscard]] virtual std::optional<std::int32_t> score(std::int32_t board, std::string_view entry,
                                                            std::string_view objective) {
        static_cast<void>(board);
        static_cast<void>(entry);
        static_cast<void>(objective);
        return std::nullopt;
    }

    virtual bool setScore(std::int32_t board, std::string_view entry, std::string_view objective, std::int32_t value) {
        static_cast<void>(board);
        static_cast<void>(entry);
        static_cast<void>(objective);
        static_cast<void>(value);
        return false;
    }

    /// Without an objective, every score of the entry goes.
    virtual void resetScore(std::int32_t board, std::string_view entry, const std::optional<std::string>& objective) {
        static_cast<void>(board);
        static_cast<void>(entry);
        static_cast<void>(objective);
    }

    [[nodiscard]] virtual std::vector<std::string> entries(std::int32_t board) {
        static_cast<void>(board);
        return {};
    }

    [[nodiscard]] virtual std::vector<std::string> entriesIn(std::int32_t board, std::string_view objective) {
        static_cast<void>(board);
        static_cast<void>(objective);
        return {};
    }

    [[nodiscard]] virtual std::vector<std::pair<std::string, std::int32_t>> scoresOf(std::int32_t board,
                                                                                     std::string_view entry) {
        static_cast<void>(board);
        static_cast<void>(entry);
        return {};
    }

    [[nodiscard]] virtual PluginScoreDisplay scoreDisplay(std::int32_t board, std::string_view entry,
                                                          std::string_view objective) {
        static_cast<void>(board);
        static_cast<void>(entry);
        static_cast<void>(objective);
        return {};
    }

    virtual void setScoreDisplay(std::int32_t board, std::string_view entry, std::string_view objective,
                                 const PluginScoreDisplay& display) {
        static_cast<void>(board);
        static_cast<void>(entry);
        static_cast<void>(objective);
        static_cast<void>(display);
    }

    /// Whether a /trigger of this objective is allowed for the entry.
    [[nodiscard]] virtual bool triggerable(std::int32_t board, std::string_view entry, std::string_view objective) {
        static_cast<void>(board);
        static_cast<void>(entry);
        static_cast<void>(objective);
        return false;
    }

    virtual void setTriggerable(std::int32_t board, std::string_view entry, std::string_view objective, bool value) {
        static_cast<void>(board);
        static_cast<void>(entry);
        static_cast<void>(objective);
        static_cast<void>(value);
    }

    [[nodiscard]] virtual std::vector<PluginTeam> teams(std::int32_t board) {
        static_cast<void>(board);
        return {};
    }

    [[nodiscard]] virtual std::optional<PluginTeam> team(std::int32_t board, std::string_view name) {
        static_cast<void>(board);
        static_cast<void>(name);
        return std::nullopt;
    }

    [[nodiscard]] virtual std::optional<PluginTeam> teamOf(std::int32_t board, std::string_view entry) {
        static_cast<void>(board);
        static_cast<void>(entry);
        return std::nullopt;
    }

    virtual bool addTeam(std::int32_t board, std::string_view name) {
        static_cast<void>(board);
        static_cast<void>(name);
        return false;
    }

    /// Changes the options of an existing team; its members are left alone.
    virtual bool updateTeam(std::int32_t board, const PluginTeam& team) {
        static_cast<void>(board);
        static_cast<void>(team);
        return false;
    }

    virtual bool removeTeam(std::int32_t board, std::string_view name) {
        static_cast<void>(board);
        static_cast<void>(name);
        return false;
    }

    /// An entry on another team leaves it first, as on the vanilla board.
    virtual bool addMember(std::int32_t board, std::string_view team, std::string_view entry) {
        static_cast<void>(board);
        static_cast<void>(team);
        static_cast<void>(entry);
        return false;
    }

    virtual bool removeMember(std::int32_t board, std::string_view team, std::string_view entry) {
        static_cast<void>(board);
        static_cast<void>(team);
        static_cast<void>(entry);
        return false;
    }

    /// Shows this board to the player instead of the one they had; the main board is the default.
    virtual void show(std::string_view uuid, std::int32_t board) {
        static_cast<void>(uuid);
        static_cast<void>(board);
    }

    [[nodiscard]] virtual std::int32_t shown(std::string_view uuid) {
        static_cast<void>(uuid);
        return kMain;
    }
};

inline PluginScoreboards& PluginScoreboards::unavailable() {
    static PluginScoreboards none;
    return none;
}

} // namespace basalt::server::plugins
