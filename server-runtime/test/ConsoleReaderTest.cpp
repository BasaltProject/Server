#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ConsoleReader.hpp>

#include <doctest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <unistd.h>

using namespace basalt::server::runtime::command;
using namespace std::chrono_literals;

TEST_CASE("noninteractive server console stops without dispatching an unterminated line") {
    const auto saved = ::dup(STDIN_FILENO);
    REQUIRE(saved >= 0);
    int input[2]{};
    REQUIRE(::pipe(input) == 0);
    REQUIRE(::dup2(input[0], STDIN_FILENO) >= 0);
    std::atomic<unsigned> dispatched{0};
    consoleReader::start([&]() -> CommandDispatcher* {
        ++dispatched;
        return nullptr;
    });
    CHECK(::write(input[1], "help", 4) == 4);
    std::this_thread::sleep_for(20ms);
    auto stopped = std::async(std::launch::async, [] { consoleReader::stop(); });
    const auto finished = stopped.wait_for(500ms);
    CHECK(finished == std::future_status::ready);
    ::close(input[1]);
    stopped.get();
    ::dup2(saved, STDIN_FILENO);
    ::close(saved);
    ::close(input[0]);
    CHECK(dispatched.load() == 0);
}
