#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/lua/LuaSandbox.hpp>

#include <doctest.h>

#include <string>
#include <thread>
#include <vector>

using namespace basalt::server::lua;

namespace {

/// Runs one chunk and answers what went wrong, or nothing.
[[nodiscard]] std::optional<LuaFailure> run(LuaSandbox& sandbox, std::string_view source) {
    const std::expected<LuaChunk, LuaFailure> chunk = sandbox.compile(source, "test.lua");
    if (!chunk.has_value()) {
        return chunk.error();
    }
    const std::expected<void, LuaFailure> result = sandbox.run(*chunk);
    if (!result.has_value()) {
        return result.error();
    }
    return std::nullopt;
}

/// Whether a global is gone, asked from inside the sandbox so nothing here can be fooled by the
/// host's own view of the state.
[[nodiscard]] bool isNil(LuaSandbox& sandbox, std::string_view expression) {
    std::vector<std::string> printed;
    const std::optional<LuaFailure> failure =
        run(sandbox, std::string{"print(("} + std::string{expression} + ") == nil)");
    return !failure.has_value();
}

} // namespace

TEST_CASE("a plugin cannot reach what the sandbox took away") {
    std::vector<std::string> printed;
    const std::unique_ptr<LuaSandbox> sandbox =
        LuaSandbox::create("arena", {}, 0, {}, [&](std::string_view line) { printed.emplace_back(line); });
    REQUIRE(sandbox != nullptr);

    // The whole removed list, asked for by name from inside.
    CHECK_FALSE(run(*sandbox, R"(
        local gone = {"io", "debug", "dofile", "loadfile", "collectgarbage", "package"}
        for _, name in ipairs(gone) do
            assert(_G[name] == nil, name .. " is still reachable")
        end
        local osGone = {"execute", "exit", "getenv", "remove", "rename", "tmpname", "setlocale"}
        for _, name in ipairs(osGone) do
            assert(os[name] == nil, "os." .. name .. " is still reachable")
        end
    )")
                    .has_value());

    // What is left is what a plugin legitimately needs.
    CHECK_FALSE(run(*sandbox, R"(
        assert(type(os.time) == "function")
        assert(type(os.date) == "function")
        assert(type(string.format) == "function")
        assert(type(table.insert) == "function")
        assert(type(math.floor) == "function")
        assert(type(pcall) == "function")
        assert(type(coroutine.create) == "function")
    )")
                    .has_value());
    CHECK(isNil(*sandbox, "io"));
}

TEST_CASE("print goes to the plugin's own log and nowhere else") {
    std::vector<std::string> printed;
    const std::unique_ptr<LuaSandbox> sandbox =
        LuaSandbox::create("arena", {}, 0, {}, [&](std::string_view line) { printed.emplace_back(line); });
    REQUIRE(sandbox != nullptr);

    CHECK_FALSE(run(*sandbox, R"(print("one", 2, true, nil))").has_value());
    REQUIRE(printed.size() == 1);
    // Tab-separated and every value turned to text, which is what Lua's own print does.
    CHECK(printed[0] == "one\t2\ttrue\tnil");
}

TEST_CASE("a runaway plugin is stopped by its instruction budget") {
    // Small enough that the loop below cannot finish inside it.
    const std::unique_ptr<LuaSandbox> sandbox = LuaSandbox::create("arena", LuaLimits{.instructionsPerTurn = 20000});
    REQUIRE(sandbox != nullptr);

    const std::optional<LuaFailure> failure = run(*sandbox, "while true do end");
    REQUIRE(failure.has_value());
    CHECK(failure->outOfBudget());
    CHECK_FALSE(failure->outOfMemory());
    CHECK(failure->plugin == "arena");
    CHECK(failure->report().starts_with("[arena] "));
    // Stopped when the budget ran out, not a long way past it: the hook has to be asked often
    // enough that a runaway loop costs a fraction of a millisecond rather than a tick.
    CHECK(sandbox->instructionsLeft() > -2000);

    // The next entry from the server is a fresh turn, so the same state runs again.
    sandbox->refill();
    CHECK(sandbox->instructionsLeft() == 20000);
    CHECK_FALSE(
        run(*sandbox, "local total = 0 for i = 1, 10 do total = total + i end assert(total == 55)").has_value());
}

TEST_CASE("a plugin stopped from another thread says so") {
    // A budget big enough that the loop below would otherwise finish, so what stops it is the
    // interrupt and not the count.
    const std::unique_ptr<LuaSandbox> sandbox =
        LuaSandbox::create("arena", LuaLimits{.instructionsPerTurn = 1000000000});
    REQUIRE(sandbox != nullptr);

    // The interrupt comes from a thread that is not the one running Lua, which is the only way it
    // ever arrives: it waits until the budget has visibly moved, so the plugin is really running.
    std::jthread stopper{[&] {
        while (sandbox->instructionsLeft() >= 1000000000) {
            std::this_thread::yield();
        }
        sandbox->interrupt();
    }};

    const std::optional<LuaFailure> failure = run(*sandbox, "local total = 0 for i = 1, 200000000 do total = i end");
    stopper.join();
    REQUIRE(failure.has_value());
    CHECK(failure->outOfBudget());
    CHECK(failure->message.find("interrupted") != std::string::npos);

    // Refilling clears it: an interrupt stops this turn, not the plugin, so the next entry from the
    // server starts clean rather than being born stopped. Long enough to reach the hook, or a stale
    // interrupt would never be noticed either way.
    sandbox->refill();
    CHECK_FALSE(run(*sandbox, "local total = 0 for i = 1, 100000 do total = total + i end").has_value());
}

TEST_CASE("a plugin cannot take more memory than it is allowed") {
    const std::unique_ptr<LuaSandbox> sandbox =
        LuaSandbox::create("arena", LuaLimits{.instructionsPerTurn = 100000000, .memoryBytes = 512 * 1024});
    REQUIRE(sandbox != nullptr);

    const std::optional<LuaFailure> failure =
        run(*sandbox, "local t = {} for i = 1, 10000000 do t[i] = string.rep('x', 64) end");
    REQUIRE(failure.has_value());
    CHECK(failure->outOfMemory());
    CHECK(sandbox->memoryUsed() <= 512 * 1024);
}

TEST_CASE("memory a plugin gives back is memory it may take again") {
    const std::unique_ptr<LuaSandbox> sandbox =
        LuaSandbox::create("arena", LuaLimits{.instructionsPerTurn = 100000000, .memoryBytes = 4 * 1024 * 1024});
    REQUIRE(sandbox != nullptr);

    // Many times the ceiling passes through the allocator, but almost none of it is live at once.
    // A tally that only ever grows would refuse this well before the end.
    CHECK_FALSE(run(*sandbox, R"(
        for round = 1, 200 do
            local scratch = {}
            for i = 1, 200 do scratch[i] = string.rep("x", 512) end
        end
    )")
                    .has_value());
    CHECK(sandbox->memoryUsed() <= 4 * 1024 * 1024);
}

TEST_CASE("load cannot bring in a binary chunk") {
    std::vector<std::string> printed;
    const std::unique_ptr<LuaSandbox> sandbox =
        LuaSandbox::create("arena", {}, 0, {}, [&](std::string_view line) { printed.emplace_back(line); });
    REQUIRE(sandbox != nullptr);

    // Text still loads, and the mode the plugin asked for is overruled rather than obeyed. The
    // chunk offered here is a real dump rather than nonsense, so what refuses it is the mode and
    // not the bytes being malformed.
    CHECK_FALSE(run(*sandbox, R"(
        local fn = load("return 7")
        assert(fn() == 7)
        local dumped = string.dump(fn)
        assert(#dumped > 0)
        local binary = load(dumped, "evil", "b")
        assert(binary == nil, "a binary chunk was loaded")
        assert(load("\27Lua nonsense", "evil", "b") == nil)
    )")
                    .has_value());
}

TEST_CASE("require sees only the finder the host installed") {
    std::vector<std::string> asked;
    const LuaSandbox::SourceFinder sources = [&](std::string_view name) -> std::optional<std::vector<std::uint8_t>> {
        asked.emplace_back(name);
        if (name == "arena.util") {
            const std::string source = "return { double = function(n) return n * 2 end }";
            return std::vector<std::uint8_t>{source.begin(), source.end()};
        }
        return std::nullopt;
    };
    const std::unique_ptr<LuaSandbox> sandbox = LuaSandbox::create("arena", {}, 0, sources);
    REQUIRE(sandbox != nullptr);

    CHECK_FALSE(run(*sandbox, R"(
        local util = require("arena.util")
        assert(util.double(21) == 42)
        -- Requiring twice hands back the same table rather than running the module again.
        assert(require("arena.util") == util)
    )")
                    .has_value());
    REQUIRE(asked.size() == 1);
    CHECK(asked[0] == "arena.util");

    // The libraries that were taken away cannot be required back in, which is the door the frozen
    // Kotlin found open behind the one it had locked.
    const std::optional<LuaFailure> failure = run(*sandbox, R"(local io = require("io"))");
    REQUIRE(failure.has_value());
    CHECK(failure->message.find("not found") != std::string::npos);
}

TEST_CASE("a module is source, never bytecode, and errors in it name the module") {
    const LuaSandbox::SourceFinder sources = [](std::string_view name) -> std::optional<std::vector<std::uint8_t>> {
        if (name == "arena.broken") {
            const std::string source = "\n\nerror('inside the module')";
            return std::vector<std::uint8_t>{source.begin(), source.end()};
        }
        if (name == "arena.dumped") {
            // A real compiled chunk offered as a module: what an attacker with write access to the
            // plugin directory would leave there, and what the loader must not take.
            static const std::vector<std::uint8_t> dumped = [] {
                const std::unique_ptr<LuaSandbox> helper = LuaSandbox::create("helper");
                const std::expected<LuaChunk, LuaFailure> chunk = helper->compile("return 7", "x.lua");
                return chunk.has_value() ? chunk->bytecode : std::vector<std::uint8_t>{};
            }();
            return dumped;
        }
        return std::nullopt;
    };
    const std::unique_ptr<LuaSandbox> sandbox = LuaSandbox::create("arena", {}, 0, sources);
    REQUIRE(sandbox != nullptr);

    const std::optional<LuaFailure> broken = run(*sandbox, R"(require("arena.broken"))");
    REQUIRE(broken.has_value());
    CHECK(broken->message.find("arena.broken:3:") != std::string::npos);

    const std::optional<LuaFailure> dumped = run(*sandbox, R"(require("arena.dumped"))");
    REQUIRE(dumped.has_value());
    CHECK(dumped->message.find("attempt to load a binary chunk") != std::string::npos);
}

TEST_CASE("a seed is what makes a plugin's randomness reproducible") {
    const auto firstDraws = [](std::uint64_t seed) {
        std::vector<std::string> printed;
        const std::unique_ptr<LuaSandbox> sandbox =
            LuaSandbox::create("arena", {}, seed, {}, [&](std::string_view line) { printed.emplace_back(line); });
        static_cast<void>(run(*sandbox, "print(math.random(1, 1000000), math.random(1, 1000000))"));
        return printed.empty() ? std::string{} : printed[0];
    };

    CHECK(firstDraws(7) == firstDraws(7));
    CHECK(firstDraws(7) != firstDraws(8));
    CHECK_FALSE(firstDraws(7).empty());
}

TEST_CASE("every run is a fresh turn, whatever the last one spent") {
    const std::unique_ptr<LuaSandbox> sandbox = LuaSandbox::create("arena", LuaLimits{.instructionsPerTurn = 400000});
    REQUIRE(sandbox != nullptr);

    // Two chunks that each need most of the turn: without a refill between them the second cannot
    // finish, because the first left almost nothing behind.
    const std::string heavy = "local total = 0 for i = 1, 100000 do total = total + i end assert(total > 0)";
    CHECK_FALSE(run(*sandbox, heavy).has_value());
    // What is left after one heavy chunk is a long way short of a whole turn, so a second one on
    // the same budget could not finish.
    const std::int64_t afterFirst = sandbox->instructionsLeft();
    CHECK(afterFirst <= 200000);
    CHECK_FALSE(run(*sandbox, heavy).has_value());
    CHECK_FALSE(run(*sandbox, heavy).has_value());
}

TEST_CASE("a compiled chunk runs in a second state") {
    const std::unique_ptr<LuaSandbox> first = LuaSandbox::create("arena");
    const std::unique_ptr<LuaSandbox> second = LuaSandbox::create("arena");
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    const std::expected<LuaChunk, LuaFailure> chunk = first->compile("counted = (counted or 0) + 1", "shared.lua");
    REQUIRE(chunk.has_value());
    CHECK_FALSE(chunk->bytecode.empty());

    // The same bytes, twice into one state and once into another: what a lane borrows is the code,
    // never the environment.
    CHECK(first->run(*chunk).has_value());
    CHECK(first->run(*chunk).has_value());
    CHECK(second->run(*chunk).has_value());
    CHECK_FALSE(run(*first, "assert(counted == 2, 'first state counted ' .. counted)").has_value());
    CHECK_FALSE(run(*second, "assert(counted == 1, 'second state counted ' .. counted)").has_value());
}

TEST_CASE("a syntax error names the plugin's own file and line") {
    const std::unique_ptr<LuaSandbox> sandbox = LuaSandbox::create("arena");
    REQUIRE(sandbox != nullptr);

    const std::expected<LuaChunk, LuaFailure> chunk = sandbox->compile("this is not lua", "main.lua");
    REQUIRE_FALSE(chunk.has_value());
    CHECK(chunk.error().plugin == "arena");
    CHECK(chunk.error().message.find("main.lua:1:") != std::string::npos);
    CHECK_FALSE(chunk.error().outOfBudget());

    const std::optional<LuaFailure> runtime = run(*sandbox, "\n\nerror('boom')");
    REQUIRE(runtime.has_value());
    CHECK(runtime->message.find("test.lua:3:") != std::string::npos);
    CHECK(runtime->report() == "[arena] " + runtime->message);
}
