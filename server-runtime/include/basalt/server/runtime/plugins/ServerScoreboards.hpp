#pragma once

#include <basalt/server/plugins/PluginScoreboards.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/scoreboard/Scoreboard.hpp>
#include <basalt/server/runtime/scoreboard/Teams.hpp>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace basalt::server::runtime::plugins {

/// The server's scoreboard and the boards plugins make, drawn to each player from the board they are shown.
class ServerScoreboards final : public server::plugins::PluginScoreboards {
public:
    struct Hooks {
        std::function<std::vector<player::PlayerSession*>()> sessions;
        scoreboard::Scoreboard* main = nullptr;
        scoreboard::Teams* mainTeams = nullptr;
    };

    explicit ServerScoreboards(Hooks hooks);

    /// Draws the main board for a player who has just joined.
    void joined(const player::PlayerSession& player);

    void left(const std::string& uuid);

    std::int32_t create() override;
    void release(std::int32_t board) override;
    [[nodiscard]] std::vector<server::plugins::PluginObjective> objectives(std::int32_t board) override;
    [[nodiscard]] std::optional<server::plugins::PluginObjective> objective(std::int32_t board,
                                                                            std::string_view name) override;
    bool addObjective(std::int32_t board, const server::plugins::PluginObjective& objective) override;
    bool updateObjective(std::int32_t board, const server::plugins::PluginObjective& objective) override;
    bool removeObjective(std::int32_t board, std::string_view name) override;
    [[nodiscard]] std::optional<std::string> displayed(std::int32_t board, std::int32_t slot) override;
    void display(std::int32_t board, std::int32_t slot, const std::optional<std::string>& objective) override;
    [[nodiscard]] std::optional<std::int32_t> score(std::int32_t board, std::string_view entry,
                                                    std::string_view objective) override;
    bool setScore(std::int32_t board, std::string_view entry, std::string_view objective, std::int32_t value) override;
    void resetScore(std::int32_t board, std::string_view entry, const std::optional<std::string>& objective) override;
    [[nodiscard]] std::vector<std::string> entries(std::int32_t board) override;
    [[nodiscard]] std::vector<std::string> entriesIn(std::int32_t board, std::string_view objective) override;
    [[nodiscard]] std::vector<std::pair<std::string, std::int32_t>> scoresOf(std::int32_t board,
                                                                             std::string_view entry) override;
    [[nodiscard]] server::plugins::PluginScoreDisplay scoreDisplay(std::int32_t board, std::string_view entry,
                                                                   std::string_view objective) override;
    void setScoreDisplay(std::int32_t board, std::string_view entry, std::string_view objective,
                         const server::plugins::PluginScoreDisplay& display) override;
    [[nodiscard]] bool triggerable(std::int32_t board, std::string_view entry, std::string_view objective) override;
    void setTriggerable(std::int32_t board, std::string_view entry, std::string_view objective, bool value) override;
    [[nodiscard]] std::vector<server::plugins::PluginTeam> teams(std::int32_t board) override;
    [[nodiscard]] std::optional<server::plugins::PluginTeam> team(std::int32_t board, std::string_view name) override;
    [[nodiscard]] std::optional<server::plugins::PluginTeam> teamOf(std::int32_t board,
                                                                    std::string_view entry) override;
    bool addTeam(std::int32_t board, std::string_view name) override;
    bool updateTeam(std::int32_t board, const server::plugins::PluginTeam& team) override;
    bool removeTeam(std::int32_t board, std::string_view name) override;
    bool addMember(std::int32_t board, std::string_view team, std::string_view entry) override;
    bool removeMember(std::int32_t board, std::string_view team, std::string_view entry) override;
    void show(std::string_view uuid, std::int32_t board) override;
    [[nodiscard]] std::int32_t shown(std::string_view uuid) override;

private:
    struct Board {
        std::unique_ptr<scoreboard::Scoreboard> ownedScores;
        std::unique_ptr<scoreboard::Teams> ownedTeams;
        scoreboard::Scoreboard* scores = nullptr;
        scoreboard::Teams* teams = nullptr;
    };

    [[nodiscard]] Board* find(std::int32_t board);
    [[nodiscard]] std::vector<player::PlayerSession*> viewersOf(std::int32_t board) const;
    void drawAll(const player::PlayerSession& player, Board& board) const;
    void eraseAll(const player::PlayerSession& player, Board& board) const;
    void send(std::int32_t board, const std::function<void(const player::PlayerSession&)>& one) const;
    void sendObjective(std::int32_t board, const scoreboard::Objective& objective, std::uint8_t method) const;
    void sendScore(std::int32_t board, std::string_view entry, std::string_view objective) const;
    void sendTeam(std::int32_t board, const scoreboard::Team& team, std::uint8_t method) const;

    Hooks hooks_;
    std::map<std::int32_t, Board> boards_;
    std::map<std::string, std::int32_t, std::less<>> viewing_;
    std::int32_t next_ = 1;
};

} // namespace basalt::server::runtime::plugins
