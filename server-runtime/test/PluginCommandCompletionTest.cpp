#include <basalt/server/runtime/plugins/ServerPluginServices.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <doctest.h>
#include <chrono>
#include <future>
#include <thread>

namespace {
using namespace basalt::server::runtime;
struct CompletionFixture {
    player::PlayerRegistry players;
    command::CommandEnvironment environment{players, {}};
    command::CommandDispatcher dispatcher{environment};
    command::ConsoleCommandSource console;
    sched::Owner owner{"completion-test"};
    plugins::DispatcherCommands commands{owner, dispatcher, [this] { return &console; }};
    CompletionFixture() { owner.bindToCurrentThread(); }
};
using Spec = basalt::server::plugins::PluginCommandSpec;
using Call = basalt::server::plugins::PluginCommandCall;
}

TEST_CASE("plugin completions expose a server provider and preserve greedy prefixes") {
    CompletionFixture f;
    auto spec = Spec::parse("probe [args:text]"); REQUIRE(spec.has_value()); spec->permission = "";
    auto handle = f.commands.registerCompletingCommand("test", *spec, [](Call&) {},
        [&f](Call& call, std::string_view input) -> std::vector<std::string> {
            CHECK(f.owner.isCurrent()); CHECK(call.source() == f.console.name()); CHECK(call.kind() == "console");
            if (input.empty()) return {"module", "reload"};
            if (input == "mo") return {"module"};
            if (input == "module s") return {"module status"};
            return {};
        });
    const auto first = f.dispatcher.suggest(f.console, "probe ");
    REQUIRE(first.suggestions.size() == 2); CHECK(first.start == 6); CHECK(first.length == 0);
    const auto partial = f.dispatcher.suggest(f.console, "probe mo");
    REQUIRE(partial.suggestions.size() == 1); CHECK(partial.suggestions[0].text == "module");
    CHECK(partial.start == 6); CHECK(partial.length == 2);
    const auto nested = f.dispatcher.suggest(f.console, "probe module s");
    REQUIRE(nested.suggestions.size() == 1); CHECK(nested.suggestions[0].text == "module status");
    CHECK(nested.start == 6); CHECK(nested.length == 8);
    const auto tree = f.dispatcher.treeFor(f.console);
    REQUIRE(tree.nodes.size() == 3);
    CHECK(tree.nodes[2].suggestionId == "minecraft:ask_server");
    handle.remove(); CHECK(f.dispatcher.suggest(f.console, "probe mo").suggestions.empty());
}

TEST_CASE("plugin completion from a network thread runs on the owner") {
    CompletionFixture f;
    auto spec = Spec::parse("probe [args:text]"); REQUIRE(spec.has_value()); spec->permission = "";
    auto handle = f.commands.registerCompletingCommand("test", *spec, [](Call&) {},
        [&f](Call&, std::string_view) { CHECK(f.owner.isCurrent()); return std::vector<std::string>{"ready"}; });
    const bool wasRunning = sched::Owner::tickThreadRunning(); sched::Owner::tickThreadRunning(true);
    auto future = std::async(std::launch::async, [&] { return f.dispatcher.suggest(f.console, "probe "); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{1};
    while (f.owner.pending() == 0 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    f.owner.runPending();
    const auto result = future.get(); sched::Owner::tickThreadRunning(wasRunning);
    REQUIRE(result.suggestions.size() == 1); CHECK(result.suggestions[0].text == "ready");
    handle.remove();
}

TEST_CASE("plugin completion is bounded when the owner does not drain") {
    CompletionFixture f;
    auto spec = Spec::parse("probe [args:text]"); REQUIRE(spec.has_value()); spec->permission = "";
    int calls = 0;
    auto handle = f.commands.registerCompletingCommand("test", *spec, [](Call&) {},
        [&calls](Call&, std::string_view) { ++calls; return std::vector<std::string>{"unexpected"}; });
    const bool wasRunning = sched::Owner::tickThreadRunning(); sched::Owner::tickThreadRunning(true);
    auto future = std::async(std::launch::async, [&] { return f.dispatcher.suggest(f.console, "probe "); });
    const auto result = future.get(); sched::Owner::tickThreadRunning(wasRunning);
    CHECK(result.suggestions.empty()); f.owner.runPending(); CHECK(calls == 0); handle.remove();
}
