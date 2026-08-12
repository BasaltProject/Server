#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/command/ConsoleReader.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>
#include <basalt/wire/io/ConsoleInput.hpp>
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

constexpr int kFirstPrintable = 32;

constexpr std::size_t kMaxHistory = 100;

constexpr std::chrono::milliseconds kPoll{25};

/// Long enough to cover a restart and a cold start; past it, something is wrong and saying so beats hanging.
constexpr std::chrono::milliseconds kMaxWait{60000};

std::atomic<bool> running{false};
std::jthread reader;

std::function<void()> redrawPrompt;

[[nodiscard]] int readByteOrStop() {
    while (running.load()) {
        if (wire::io::terminal::waitForInput(kPoll)) {
            return wire::io::terminal::readByte();
        }
    }
    return -1;
}

[[nodiscard]] std::optional<std::string> readEditedLine(const std::vector<std::string>& history) {
    std::string line;
    std::size_t cursor = 0;
    std::size_t historyIndex = history.size();
    std::string draft;

    const auto redraw = [&line, &cursor] {
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

void readLoop(const std::function<CommandDispatcher*()>& current, bool editing, std::stop_token stop) {
    wire::io::ConsoleInput input{65536};
    std::vector<std::string> history;
    while (running.load()) {
        std::optional<std::string> line;
        if (editing) {
            line = readEditedLine(history);
        } else {
            auto read = input.readLine(stop);
            if (read) {
                line = std::move(*read);
            } else if (read.error() == wire::io::ConsoleInputError::kTooLong) {
                std::cout << "Console command exceeds 65536 bytes; line discarded.\n";
                continue;
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
        // A command from the console reaches the world, so it runs on whoever owns it rather than on this thread.
        const CommandEnvironment& environment = dispatcher->environment();
        const auto named = environment.options().worlds.find(environment.options().defaultWorldName);
        world::LoadedWorld* defaultWorld = named == environment.options().worlds.end() ? nullptr : named->second;
        auto source = std::make_shared<ConsoleCommandSource>(
            wire::common::Logger{"Console"}, environment.lang(), [defaultWorld]() -> std::array<double, 3> {
                return defaultWorld == nullptr ? std::array<double, 3>{0.0, 0.0, 0.0} : defaultWorld->spawn();
            });
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
            if (asked != nullptr) {
                if (const auto* edited = std::get_if<std::string>(&asked->get("command")); edited != nullptr) {
                    line = *edited;
                }
            }
            static_cast<void>(dispatcher->execute(*source, line));
        };
        if (defaultWorld == nullptr) {
            run();
        } else {
            static_cast<void>(defaultWorld->owner().submitConsole(run));
        }
    }
}

} // namespace

void start(std::function<CommandDispatcher*()> current) {
    bool expected = false;
    if (!running.compare_exchange_strong(expected, true)) {
        return;
    }
    reader = std::jthread([current = std::move(current)](std::stop_token stop) {
        const bool editing = wire::io::terminal::enableRawMode();
        if (editing) {
            // Log lines and the prompt share one terminal.
            wire::common::logOutput::setAroundConsoleLine([](const std::function<void()>& write) {
                std::fputs("\r\033[K", stdout);
                write();
                if (redrawPrompt) {
                    redrawPrompt();
                }
                wire::io::flushConsole();
            });
        }
        readLoop(current, editing, stop);
        wire::common::logOutput::setAroundConsoleLine({});
        wire::io::terminal::restoreMode();
    });
}

void stop() {
    running.store(false);
    reader.request_stop();
    if (reader.joinable()) {
        reader.join();
    }
}

} // namespace basalt::server::runtime::command::consoleReader
