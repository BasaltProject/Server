#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `TagCommand`.
///
/// Tags live on the live session, so they are lost on disconnect. Vanilla persists them with the
/// entity; here that needs a column in `players.db`, which is a schema change and a separate piece
/// of work: stated rather than quietly half-done.
class TagCommand final : public Command {
public:
    TagCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `SpreadPlayersCommand`, including its relaxation loop: positions start random inside the
/// circle and are then pushed apart until every pair is at least `spreadDistance` apart or the
/// attempts run out, which is exactly why vanilla can fail with "too many players".
///
/// `under <maxHeight>` is accepted and honoured as a ceiling on the landing height; teams are not,
/// because spreading by team needs the team of an entity that is not a player.
class SpreadPlayersCommand final : public Command {
public:
    explicit SpreadPlayersCommand(std::function<double()> nextDouble);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<double()> nextDouble_;
};

} // namespace basalt::server::runtime::command::commands
