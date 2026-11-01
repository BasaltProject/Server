#pragma once

#include <functional>

namespace basalt::server::runtime::command {

class CommandDispatcher;

namespace consoleReader {

/// Started once per process; input waits observe shutdown even with a partial piped line.
void start(std::function<CommandDispatcher*()> current);

/// From here until `markReady`, console lines are held in order instead of run.
void beginStartup();

/// Runs what was held while starting, then lets new lines through.
void markReady();

void stop();

} // namespace consoleReader

} // namespace basalt::server::runtime::command
