#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/lua/LuaCall.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::lua;

namespace {

/// Loads a chunk and leaves the function it makes on the sandbox's stack, ready for a call.
[[nodiscard]] bool push(LuaSandbox& sandbox, std::string_view source) {
    const std::expected<LuaChunk, LuaFailure> chunk = sandbox.compile(source, "test.lua");
    return chunk.has_value() && sandbox.bind(*chunk).has_value();
}

} // namespace

TEST_CASE("a call that never waits finishes before it returns") {
    std::vector<std::string> printed;
    const std::unique_ptr<LuaSandbox> sandbox =
        LuaSandbox::create("arena", {}, 0, {}, [&](std::string_view line) { printed.emplace_back(line); });
    REQUIRE(sandbox != nullptr);
    REQUIRE(push(*sandbox, R"(print("ran"))"));

    std::int32_t done = 0;
    std::optional<LuaFailure> reported;
    const std::shared_ptr<LuaCall> call = LuaCall::start(*sandbox, 0, [&](std::optional<LuaFailure> failure) {
        ++done;
        reported = std::move(failure);
    });

    CHECK(done == 1);
    CHECK_FALSE(reported.has_value());
    CHECK(call->finished());
    CHECK_FALSE(call->paused());
    REQUIRE(printed.size() == 1);
    CHECK(printed[0] == "ran");
}

TEST_CASE("a failing call reports the plugin's own message once") {
    const std::unique_ptr<LuaSandbox> sandbox = LuaSandbox::create("arena");
    REQUIRE(sandbox != nullptr);
    REQUIRE(push(*sandbox, "\nerror('boom')"));

    std::int32_t done = 0;
    std::optional<LuaFailure> reported;
    const std::shared_ptr<LuaCall> call = LuaCall::start(*sandbox, 0, [&](std::optional<LuaFailure> failure) {
        ++done;
        reported = std::move(failure);
    });

    CHECK(done == 1);
    REQUIRE(reported.has_value());
    CHECK(reported->plugin == "arena");
    CHECK(reported->message.find("test.lua:2:") != std::string::npos);
    CHECK(call->finished());
}

TEST_CASE("a plugin that waits gives the turn back and is picked up later") {
    std::vector<std::string> printed;
    const std::unique_ptr<LuaSandbox> sandbox =
        LuaSandbox::create("arena", {}, 0, {}, [&](std::string_view line) { printed.emplace_back(line); });
    REQUIRE(sandbox != nullptr);

    // What a scheduler would hold: the rest of the plugin's turn, to be run when the tick it asked
    // for comes round.
    std::vector<LuaCall::Resume> pending;
    installWaitingFunction(*sandbox, "wait", [&](LuaCall::Resume resume) { pending.push_back(std::move(resume)); });

    REQUIRE(push(*sandbox, R"(
        print("before")
        wait(20)
        print("after")
        wait(20)
        print("done")
    )"));

    std::int32_t done = 0;
    const std::shared_ptr<LuaCall> call = LuaCall::start(*sandbox, 0, [&](std::optional<LuaFailure>) { ++done; });

    // The turn ended at the first wait, with the rest of it in the scheduler's hands.
    CHECK(done == 0);
    CHECK_FALSE(call->finished());
    CHECK(call->paused());
    REQUIRE(printed.size() == 1);
    CHECK(printed[0] == "before");
    REQUIRE(pending.size() == 1);

    pending.front()();
    CHECK(done == 0);
    REQUIRE(printed.size() == 2);
    CHECK(printed[1] == "after");
    REQUIRE(pending.size() == 2);

    pending[1]();
    CHECK(done == 1);
    CHECK(call->finished());
    REQUIRE(printed.size() == 3);
    CHECK(printed[2] == "done");
}

TEST_CASE("a turn nobody resumes is simply dropped") {
    const std::unique_ptr<LuaSandbox> sandbox = LuaSandbox::create("arena");
    REQUIRE(sandbox != nullptr);
    std::vector<LuaCall::Resume> pending;
    installWaitingFunction(*sandbox, "wait", [&](LuaCall::Resume resume) { pending.push_back(std::move(resume)); });
    REQUIRE(push(*sandbox, "wait(20) error('never reached')"));

    std::int32_t done = 0;
    const std::shared_ptr<LuaCall> call = LuaCall::start(*sandbox, 0, [&](std::optional<LuaFailure>) { ++done; });
    CHECK(call->paused());
    // Never calling the resume is how an unloaded plugin stops: there is nothing to kill, and
    // nothing runs afterwards.
    pending.clear();
    CHECK(done == 0);
    CHECK_FALSE(call->finished());
}

TEST_CASE("waiting where the interpreter cannot pause is refused, not blocked") {
    const std::unique_ptr<LuaSandbox> sandbox = LuaSandbox::create("arena");
    REQUIRE(sandbox != nullptr);
    std::int32_t arranged = 0;
    installWaitingFunction(*sandbox, "wait", [&](LuaCall::Resume) { ++arranged; });

    // A sort comparator is reached through the interpreter's synchronous path, so there is no
    // coroutine to unwind to.
    REQUIRE(push(*sandbox, R"(
        local values = {3, 1, 2}
        table.sort(values, function(a, b) wait(1) return a < b end)
    )"));

    std::optional<LuaFailure> reported;
    static_cast<void>(
        LuaCall::start(*sandbox, 0, [&](std::optional<LuaFailure> failure) { reported = std::move(failure); }));
    REQUIRE(reported.has_value());
    CHECK(reported->message.find("'wait' cannot be used from here") != std::string::npos);
    CHECK(arranged == 0);
}

TEST_CASE("a host function answers straight away and sees its arguments") {
    const std::unique_ptr<LuaSandbox> sandbox = LuaSandbox::create("arena");
    REQUIRE(sandbox != nullptr);
    std::vector<std::string> seen;
    installHostFunction(*sandbox, "broadcast", [&](LuaArgs& args) {
        seen.push_back(args.checkString(1));
        args.pushInteger(static_cast<std::int64_t>(seen.size()));
        return 1;
    });

    REQUIRE(push(*sandbox, R"(
        assert(broadcast("hello") == 1)
        assert(broadcast("again") == 2)
    )"));
    std::optional<LuaFailure> reported;
    static_cast<void>(
        LuaCall::start(*sandbox, 0, [&](std::optional<LuaFailure> failure) { reported = std::move(failure); }));
    CHECK_FALSE(reported.has_value());
    REQUIRE(seen.size() == 2);
    CHECK(seen[0] == "hello");
    CHECK(seen[1] == "again");
}

TEST_CASE("a finished call is let go of, and a paused one is held onto") {
    const std::unique_ptr<LuaSandbox> sandbox = LuaSandbox::create("arena");
    REQUIRE(sandbox != nullptr);
    std::vector<LuaCall::Resume> pending;
    installWaitingFunction(*sandbox, "wait", [&](LuaCall::Resume resume) { pending.push_back(std::move(resume)); });

    std::weak_ptr<LuaCall> watched;
    {
        REQUIRE(push(*sandbox, "wait(1)"));
        const std::shared_ptr<LuaCall> call = LuaCall::start(*sandbox, 0, [](std::optional<LuaFailure>) {});
        watched = call;
        REQUIRE(call->paused());
    }
    // Paused: the state is holding the rest of the turn, so letting go of the handle changes
    // nothing. This is what makes `pending.front()()` below safe from a scheduler that kept only
    // the resume.
    CHECK_FALSE(watched.expired());

    // A lot of allocation while it is paused, which is where a thread nobody anchored would be
    // collected out from under the pause.
    REQUIRE(push(*sandbox, "for round = 1, 50 do local t = {} for i = 1, 500 do t[i] = string.rep('x', 64) end end"));
    static_cast<void>(LuaCall::start(*sandbox, 0, [](std::optional<LuaFailure>) {}));

    REQUIRE(pending.size() == 1);
    pending.front()();
    pending.clear();
    // Finished: nothing holds it any more, so the last handle really was the last one.
    CHECK(watched.expired());
}

TEST_CASE("a host function is told how many arguments it was given") {
    const std::unique_ptr<LuaSandbox> sandbox = LuaSandbox::create("arena");
    REQUIRE(sandbox != nullptr);
    std::vector<std::int32_t> counts;
    installHostFunction(*sandbox, "arity", [&](LuaArgs& args) {
        counts.push_back(args.count());
        args.pushInteger(args.count());
        return 1;
    });

    REQUIRE(push(*sandbox, R"(
        assert(arity() == 0)
        assert(arity("a") == 1)
        assert(arity("a", 2, true) == 3)
    )"));
    std::optional<LuaFailure> reported;
    static_cast<void>(
        LuaCall::start(*sandbox, 0, [&](std::optional<LuaFailure> failure) { reported = std::move(failure); }));
    CHECK_FALSE(reported.has_value());
    REQUIRE(counts.size() == 3);
    CHECK(counts[0] == 0);
    CHECK(counts[2] == 3);
}

TEST_CASE("entering a call is a fresh turn, whatever the last one spent") {
    const std::unique_ptr<LuaSandbox> sandbox = LuaSandbox::create("arena", LuaLimits{.instructionsPerTurn = 400000});
    REQUIRE(sandbox != nullptr);

    // Spend most of the turn outside the call, so what is left could not carry the call below.
    const std::expected<LuaChunk, LuaFailure> heavy =
        sandbox->compile("local total = 0 for i = 1, 100000 do total = total + i end", "heavy.lua");
    REQUIRE(heavy.has_value());
    REQUIRE(sandbox->run(*heavy).has_value());
    REQUIRE(sandbox->run(*heavy).has_value());
    CHECK(sandbox->instructionsLeft() <= 200000);

    REQUIRE(push(*sandbox, "local total = 0 for i = 1, 100000 do total = total + i end assert(total > 0)"));
    std::optional<LuaFailure> reported;
    static_cast<void>(
        LuaCall::start(*sandbox, 0, [&](std::optional<LuaFailure> failure) { reported = std::move(failure); }));
    CHECK_FALSE(reported.has_value());
}

TEST_CASE("a call is given its arguments and its own budget") {
    const std::unique_ptr<LuaSandbox> sandbox = LuaSandbox::create("arena", LuaLimits{.instructionsPerTurn = 400000});
    REQUIRE(sandbox != nullptr);
    std::vector<std::string> printed;

    const std::expected<LuaChunk, LuaFailure> chunk =
        sandbox->compile("local a, b = ... return assert(a + b == 30)", "args.lua");
    REQUIRE(chunk.has_value());

    for (std::int32_t round = 0; round < 3; ++round) {
        REQUIRE(sandbox->bind(*chunk).has_value());
        LuaArgs stack{sandbox->state()};
        stack.pushInteger(10);
        stack.pushInteger(20);
        std::optional<LuaFailure> reported;
        static_cast<void>(
            LuaCall::start(*sandbox, 2, [&](std::optional<LuaFailure> failure) { reported = std::move(failure); }));
        CHECK_FALSE(reported.has_value());
    }
}
