#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/command/ConsoleReader.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>
#include <basalt/wire/io/Platform.hpp>

#include <atomic>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace basalt::server::runtime::command::consoleReader {
namespace {

constexpr int kCarriageReturn = 13;
constexpr int kNewline = 10;
constexpr int kBackspace = 8;
constexpr int kDelete = 127;
constexpr int kEscape = 27;
constexpr int kBracket = 91;
constexpr int kUp = 65;
constexpr int kDown = 66;
constexpr int kRight = 67;
constexpr int kLeft = 68;

/// Below this are control characters, which this editor does not act on.
constexpr int kFirstPrintable = 32;

/// Enough to scroll back through a session's worth of commands without growing without bound.
constexpr std::size_t kMaxHistory = 100;

constexpr std::chrono::milliseconds kPoll{25};

/// Long enough to cover a restart and a cold start; past it, something is wrong and saying so beats
/// hanging.
constexpr std::chrono::milliseconds kMaxWait{60000};

std::atomic<bool> running{false};
std::jthread reader;

/// How the log hook puts the prompt back. Set while a line is being edited and cleared between
/// lines, so a log line arriving with nothing half-typed prints on its own.
std::function<void()> redrawPrompt;

/// A byte from standard input, or -1 when input ended or the reader was told to stop. Waiting in
/// slices is what lets `stop` be noticed: a plain read parks inside the call until input arrives,
/// and stdin held open by a supervisor or a fifo never delivers any.
[[nodiscard]] int readByteOrStop() {
    while (running.load()) {
        if (wire::io::terminal::waitForInput(kPoll)) {
            return wire::io::terminal::readByte();
        }
    }
    return -1;
}

/// One line, edited in place: printable characters, backspace, left and right, and up and down
/// through the history. Answers nothing at end of input.
///
/// Deliberately small. This is not a readline implementation and does not try to be: no completion,
/// no word motions, no search, no multi-line. It covers what an operator actually reaches for at a
/// server console, which is recalling the last command and fixing a typo in it.
[[nodiscard]] std::optional<std::string> readEditedLine(const std::vector<std::string>& history) {
    std::string line;
    std::size_t cursor = 0;
    // Where up and down currently are in the history. Past the end means "the line being typed",
    // which is what coming back down from the history has to restore.
    std::size_t historyIndex = history.size();
    std::string draft;

    const auto redraw = [&line, &cursor] {
        // Carriage return, the prompt, the line, then clear whatever the old line left behind.
        std::fputs(("\r> " + line + "\033[K").c_str(), stdout);
        if (cursor < line.size()) {
            std::fputs(("\033[" + std::to_string(line.size() - cursor) + "D").c_str(), stdout);
        }
        wire::io::flushConsole();
    };

    redrawPrompt = redraw;
    redraw();
    while (true) {
        const int byte = readByteOrStop();
        if (byte == -1) {
            redrawPrompt = nullptr;
            return std::nullopt;
        }
        if (byte == kCarriageReturn || byte == kNewline) {
            redrawPrompt = nullptr;
            std::fputs("\r\n", stdout);
            wire::io::flushConsole();
            return line;
        }
        if (byte == kBackspace || byte == kDelete) {
            if (cursor > 0) {
                line.erase(cursor - 1, 1);
                cursor--;
                redraw();
            }
            continue;
        }
        if (byte == kEscape) {
            // An arrow is escape, then a bracket, then a letter. Anything else is left alone.
            if (wire::io::terminal::readByte() != kBracket) {
                continue;
            }
            const int arrow = wire::io::terminal::readByte();
            if (arrow == kUp && historyIndex > 0) {
                if (historyIndex == history.size()) {
                    draft = line;
                }
                historyIndex--;
                line = history[historyIndex];
                cursor = line.size();
                redraw();
            } else if (arrow == kDown && historyIndex < history.size()) {
                historyIndex++;
                line = historyIndex == history.size() ? draft : history[historyIndex];
                cursor = line.size();
                redraw();
            } else if (arrow == kRight && cursor < line.size()) {
                cursor++;
                redraw();
            } else if (arrow == kLeft && cursor > 0) {
                cursor--;
                redraw();
            }
            continue;
        }
        if (byte < kFirstPrintable) {
            continue;
        }
        line.insert(cursor, 1, static_cast<char>(byte));
        cursor++;
        redraw();
    }
}

/// Waits for a server to exist, since one is either starting or being replaced right now.
[[nodiscard]] CommandDispatcher* awaitDispatcher(const std::function<CommandDispatcher*()>& current) {
    std::chrono::milliseconds waited{0};
    while (waited < kMaxWait && running.load()) {
        if (CommandDispatcher* dispatcher = current(); dispatcher != nullptr) {
            return dispatcher;
        }
        wire::io::sleepFor(kPoll);
        waited += kPoll;
    }
    return current();
}

void readLoop(const std::function<CommandDispatcher*()>& current, bool editing) {
    std::vector<std::string> history;
    while (running.load()) {
        std::optional<std::string> line;
        if (editing) {
            line = readEditedLine(history);
        } else {
            while (running.load() && !wire::io::terminal::waitForInput(kPoll)) {
            }
            std::string read;
            if (running.load() && std::getline(std::cin, read)) {
                line = read;
            }
        }
        if (!line.has_value()) {
            break;
        }
        std::string command = *line;
        const std::size_t first = command.find_first_not_of(" \t\r\n");
        const std::size_t last = command.find_last_not_of(" \t\r\n");
        command = first == std::string::npos ? "" : command.substr(first, last - first + 1);
        if (command.starts_with('/')) {
            command.erase(0, 1);
        }
        if (command.empty()) {
            continue;
        }
        // Kept as typed, and never twice in a row: a history full of repeated `list` is a history
        // you have to scroll past rather than one you can use.
        if (history.empty() || history.back() != command) {
            history.push_back(command);
        }
        if (history.size() > kMaxHistory) {
            history.erase(history.begin());
        }
        CommandDispatcher* dispatcher = awaitDispatcher(current);
        if (dispatcher == nullptr) {
            std::cout << "No server is running, so '" << command << "' was not run.\n";
            continue;
        }
        // A command from the console reaches the world, so it runs on whoever owns it rather than on
        // this thread. With nothing loaded there is nobody to ask, and it runs here.
        const CommandEnvironment& environment = dispatcher->environment();
        const auto named = environment.options().worlds.find(environment.options().defaultWorldName);
        world::LoadedWorld* defaultWorld = named == environment.options().worlds.end() ? nullptr : named->second;
        auto source = std::make_shared<ConsoleCommandSource>(
            wire::common::Logger{"Console"}, environment.lang(), [defaultWorld]() -> std::array<double, 3> {
                return defaultWorld == nullptr ? std::array<double, 3>{0.0, 0.0, 0.0} : defaultWorld->spawn();
            });
        // The plugin is asked *inside* the task, not here: a decider answers where plugins run, and
        // this thread is the one reading standard input.
        const std::function<void()> run = [dispatcher, source, command, &environment] {
            std::string line = command;
            server::plugins::PluginEventBus* bus = environment.pluginEvents();
            const server::plugins::PluginEvent* asked = server::plugins::emit(
                bus, bus == nullptr ? nullptr : &server::plugins::pluginEventTypes::serverCommand(),
                [&line](server::plugins::PluginEvent& event) {
                    event.set(0, std::string{"console"});
                    event.set(1, line);
                });
            if (asked != nullptr && asked->cancelled()) {
                const std::optional<std::string> why = asked->cancelMessage();
                if (why.has_value()) {
                    std::cout << *why << "\n";
                }
                return;
            }
            static_cast<void>(dispatcher->execute(*source, line));
        };
        if (defaultWorld == nullptr) {
            run();
        } else {
            static_cast<void>(defaultWorld->owner().submit(run));
        }
    }
}

} // namespace

void start(std::function<CommandDispatcher*()> current) {
    bool expected = false;
    if (!running.compare_exchange_strong(expected, true)) {
        return;
    }
    reader = std::jthread([current = std::move(current)] {
        // Raw mode is what makes an arrow key reach this program at all; without a terminal it is
        // unavailable and unnecessary, and plain line reading is used instead.
        const bool editing = wire::io::terminal::enableRawMode();
        if (editing) {
            // Log lines and the prompt share one terminal. Wipe the prompt before a line is written
            // and draw it again after, or every log line leaves a copy of it behind.
            wire::common::logOutput::setAroundConsoleLine([](const std::function<void()>& write) {
                std::fputs("\r\033[K", stdout);
                write();
                if (redrawPrompt) {
                    redrawPrompt();
                }
                wire::io::flushConsole();
            });
        }
        readLoop(current, editing);
        wire::common::logOutput::setAroundConsoleLine({});
        wire::io::terminal::restoreMode();
    });
}

void stop() {
    running.store(false);
    if (reader.joinable()) {
        reader.join();
    }
}

} // namespace basalt::server::runtime::command::consoleReader
