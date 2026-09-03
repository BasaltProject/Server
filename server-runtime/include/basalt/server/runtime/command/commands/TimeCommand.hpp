#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `TimeCommand` as 26.2 rewrote it: clocks, not "day time". `set day`/`noon`/`night` are
/// markers on the `minecraft:day` timeline and move the clock to the next occurrence, and a
/// dimension with no clock refuses by name.
///
/// `/time of <clock>` has to name the source world's own clock: clocks belong to worlds here, not to
/// a manager shared by every dimension.
class TimeCommand final : public Command {
public:
    TimeCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
