#pragma once

#include <functional>

namespace basalt::server::runtime::command {

class CommandDispatcher;

/// Reads operator commands from standard input on their own thread and runs them at the owner's
/// level, as in vanilla; a leading slash is accepted and ignored because operators type it anyway.
///
/// Reading standard input does not contradict the no-prompt EULA rule: that one is about never
/// blocking startup on input. At end of input the loop stops reading rather than spinning.
namespace consoleReader {

/// Started once for the life of the process, not once per server: this thread parks in a blocking
/// read nothing can interrupt, so a second reader would take alternate lines from the same terminal.
/// `current` is therefore asked for the dispatcher per line, or a restart would leave it running
/// commands against a closed database.
void start(std::function<CommandDispatcher*()> current);

/// Stops the reader at the next line, for a test and for an orderly shutdown.
void stop();

} // namespace consoleReader

} // namespace basalt::server::runtime::command
