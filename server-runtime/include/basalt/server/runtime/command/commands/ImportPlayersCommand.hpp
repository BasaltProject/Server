#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::runtime::command::commands {

/// `import-players <path>`: one-way import of an existing vanilla server's players into
/// `players.db`.
///
/// No vanilla counterpart, so no parity claim and no translation key: the messages are literal
/// because a client would have nothing to resolve them against. Owner level, because it writes to
/// every row in the database.
///
/// The path is a greedy string on purpose. A save directory is very often somewhere with a space in
/// it, and quoting is the kind of thing an operator gets wrong once and then blames the server for.
class ImportPlayersCommand final : public Command {
public:
    explicit ImportPlayersCommand(std::function<std::int64_t()> nowEpochSeconds);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<std::int64_t()> nowEpochSeconds_;
};

} // namespace basalt::server::runtime::command::commands
