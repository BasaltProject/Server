#pragma once

#include <functional>

namespace basalt::server::runtime::command {

class CommandDispatcher;

namespace consoleReader {

/// Started once per process; input waits observe shutdown even with a partial piped line.
void start(std::function<CommandDispatcher*()> current);

void stop();

} // namespace consoleReader

} // namespace basalt::server::runtime::command
