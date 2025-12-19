#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// `/placeholder`: what the registry knows and what a piece of text turns into, modelled on
/// PlaceholderAPI's debugging commands, which operators already know.
///
/// Two deliberate differences: `cmdparse` runs the command as whoever typed it, since running text
/// as the console turns permission to debug placeholders into permission to do everything, and the
/// subject must be online, a stored row not being able to answer where it is standing.
class PlaceholderCommand final : public Command {
public:
    PlaceholderCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
