#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/sched/Owner.hpp>

#include <doctest.h>

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace basalt::server::runtime::sched;

namespace {

struct StrictMode {
    explicit StrictMode(bool strict) : previous_(ownership::isStrict()) { ownership::setStrict(strict); }

    StrictMode(const StrictMode&) = delete;
    StrictMode& operator=(const StrictMode&) = delete;

    ~StrictMode() { ownership::setStrict(previous_); }

private:
    bool previous_;
};

struct Collected {
    std::vector<std::string> messages;

    [[nodiscard]] FailureHandler handler() {
        return [this](const std::exception& failure) { messages.emplace_back(failure.what()); };
    }
};

} // namespace

TEST_CASE("an unclaimed owner runs what it is given there and then") {
    Owner owner{"world"};
    CHECK_FALSE(owner.isClaimed());
    CHECK_FALSE(owner.isClosed());

    std::int32_t ran = 0;
    owner.submit([&ran] { ++ran; });
    CHECK(ran == 1);
    CHECK(owner.pending() == 0);
    CHECK(owner.runPending() == 0);
}

TEST_CASE("a claimed owner runs its own work inline and everyone else's in turn") {
    Owner owner{"world"};
    owner.bindToCurrentThread();
    CHECK(owner.isClaimed());
    CHECK(owner.isCurrent());

    std::int32_t inline_ = 0;
    owner.submit([&inline_] { ++inline_; });
    CHECK(inline_ == 1);
    CHECK(owner.pending() == 0);

    std::atomic<std::int32_t> queued{0};
    std::thread other{[&owner, &queued] {
        CHECK_FALSE(owner.isCurrent());
        owner.submit([&queued] { queued.fetch_add(1); });
    }};
    other.join();
    CHECK(queued.load() == 0);
    CHECK(owner.pending() == 1);
    CHECK(owner.runPending() == 1);
    CHECK(queued.load() == 1);
    CHECK(owner.pending() == 0);
}

TEST_CASE("what a task adds belongs to the next turn, which is what bounds one") {
    Owner owner{"world"};
    owner.bindToCurrentThread();

    std::int32_t ran = 0;
    std::thread{[&owner, &ran] {
        owner.submit([&owner, &ran] {
            ++ran;
            owner.submit([&ran] { ++ran; });
        });
    }}.join();
    CHECK(owner.runPending() == 1);
    CHECK(ran == 2);
    CHECK(owner.runPending() == 0);
}

TEST_CASE("a task that drains its own owner does not take the turn it is standing in with it") {
    Owner owner{"world"};
    owner.bindToCurrentThread();

    // Saving drains the owner and `/stop` saves from a task on it; a retained turn buffer was swapped back and freed.
    std::int32_t first = 0;
    std::int32_t second = 0;
    std::thread{[&owner, &first, &second] {
        owner.submit([&owner, &first] {
            ++first;
            static_cast<void>(owner.runPending());
        });
        owner.submit([&second] { ++second; });
    }}.join();
    CHECK(owner.runPending() == 2);
    CHECK(first == 1);
    CHECK(second == 1);
    CHECK(owner.pending() == 0);
}

TEST_CASE("a cancelled task does not run, whether it was queued, delayed or at the end") {
    Owner owner{"world"};
    owner.bindToCurrentThread();
    std::int32_t ran = 0;

    TaskHandle queued;
    std::thread{[&] { queued = owner.submit([&ran] { ++ran; }); }}.join();
    CHECK_FALSE(queued.isCancelled());
    queued.cancel();
    CHECK(queued.isCancelled());
    CHECK(owner.runPending() == 0);
    CHECK(ran == 0);

    const TaskHandle delayed = owner.schedule(0, [&ran] { ++ran; });
    delayed.cancel();
    owner.advanceTick();
    CHECK(owner.runPending() == 0);
    CHECK(owner.pending() == 0);

    const TaskHandle waiting = owner.schedule(20, [&ran] { ++ran; });
    CHECK(owner.pending() == 1);
    waiting.cancel();
    owner.advanceTick();
    CHECK(owner.pending() == 0);

    const TaskHandle atEnd = owner.scheduleAtEnd([&ran] { ++ran; });
    atEnd.cancel();
    CHECK(owner.runEndOfTick() == 0);
    CHECK(ran == 0);

    TaskHandle shared;
    std::thread{[&] { shared = owner.submit([&ran] { ++ran; }); }}.join();
    const TaskHandle copy = shared;
    copy.cancel();
    CHECK(shared.isCancelled());
    CHECK(owner.runPending() == 0);
}

TEST_CASE("a delay is counted in turns, and zero means the next one") {
    Owner owner{"world"};
    owner.bindToCurrentThread();
    std::vector<std::string> ran;

    owner.schedule(0, [&ran] { ran.emplace_back("now"); });
    owner.schedule(2, [&ran] { ran.emplace_back("later"); });
    owner.schedule(-5, [&ran] { ran.emplace_back("negative"); });
    CHECK(owner.pending() == 3);

    CHECK(owner.runPending() == 0);
    CHECK(ran.empty());

    owner.advanceTick();
    CHECK(owner.runPending() == 2);
    CHECK(ran == std::vector<std::string>{"now", "negative"});

    owner.advanceTick();
    CHECK(owner.runPending() == 0);
    owner.advanceTick();
    CHECK(owner.runPending() == 1);
    CHECK(ran == std::vector<std::string>{"now", "negative", "later"});
    CHECK(owner.pending() == 0);
}

TEST_CASE("the end of a turn runs after the turn's own work") {
    Owner owner{"world"};
    owner.bindToCurrentThread();
    std::vector<std::string> ran;

    owner.scheduleAtEnd([&ran] { ran.emplace_back("end"); });
    std::thread{[&] { owner.submit([&ran] { ran.emplace_back("start"); }); }}.join();
    CHECK(owner.pending() == 2);

    CHECK(owner.runPending() == 1);
    CHECK(ran == std::vector<std::string>{"start"});
    CHECK(owner.runEndOfTick() == 1);
    CHECK(ran == std::vector<std::string>{"start", "end"});
    CHECK(owner.runEndOfTick() == 0);
}

TEST_CASE("one task that throws costs itself and nothing else") {
    Owner owner{"world"};
    owner.bindToCurrentThread();
    Collected failures;
    std::int32_t ran = 0;

    std::thread{[&] {
        owner.submit([] { throw std::runtime_error{"first"}; });
        owner.submit([&ran] { ++ran; });
        owner.submit([] { throw std::runtime_error{"second"}; });
        owner.submit([&ran] { ++ran; });
    }}.join();

    CHECK(owner.runPending(failures.handler()) == 2);
    CHECK(ran == 2);
    CHECK(failures.messages == std::vector<std::string>{"first", "second"});

    std::thread{[&] { owner.submit([] { throw std::runtime_error{"loud"}; }); }}.join();
    CHECK_THROWS_AS(owner.runPending(), std::runtime_error);
}

TEST_CASE("a closed owner runs nothing and hands back cancelled handles") {
    Owner owner{"world"};
    owner.bindToCurrentThread();
    std::int32_t ran = 0;
    TaskHandle queued;
    std::thread{[&] { queued = owner.submit([&ran] { ++ran; }); }}.join();
    const TaskHandle delayed = owner.schedule(5, [&ran] { ++ran; });
    const TaskHandle atEnd = owner.scheduleAtEnd([&ran] { ++ran; });

    owner.close();
    CHECK(owner.isClosed());
    CHECK(queued.isCancelled());
    CHECK(delayed.isCancelled());
    CHECK(atEnd.isCancelled());
    CHECK(owner.pending() == 0);
    CHECK(owner.runPending() == 0);
    CHECK(owner.runEndOfTick() == 0);
    CHECK(ran == 0);

    const TaskHandle after = owner.submit([&ran] { ++ran; });
    CHECK(after.isCancelled());
    CHECK(owner.schedule(1, [&ran] { ++ran; }).isCancelled());
    CHECK(owner.scheduleAtEnd([&ran] { ++ran; }).isCancelled());
    CHECK(ran == 0);
    CHECK(owner.pending() == 0);
}

TEST_CASE("a merge moves the work queued on the owner that goes away") {
    Owner gone{"gone"};
    Owner survivor{"survivor"};
    gone.bindToCurrentThread();
    survivor.bindToCurrentThread();
    std::vector<std::string> ran;

    std::thread{[&] {
        gone.submit([&ran] { ran.emplace_back("queued"); });
        gone.scheduleAtEnd([&ran] { ran.emplace_back("at end"); });
    }}.join();
    gone.schedule(3, [&ran] { ran.emplace_back("delayed"); });
    const TaskHandle cancelled = gone.schedule(1, [&ran] { ran.emplace_back("cancelled"); });
    cancelled.cancel();
    CHECK(gone.pending() == 4);

    gone.drainInto(survivor);
    CHECK(gone.pending() == 0);
    CHECK(survivor.pending() == 3);

    CHECK(survivor.runPending() == 2);
    CHECK(ran == std::vector<std::string>{"queued", "at end"});
    for (std::int32_t turn = 0; turn < 4; ++turn) {
        survivor.advanceTick();
        survivor.runPending();
    }
    CHECK(ran == std::vector<std::string>{"queued", "at end", "delayed"});

    Owner empty{"empty"};
    empty.drainInto(survivor);
    CHECK(survivor.pending() == 0);
}

TEST_CASE("console input waits for the turn's console point, not its start") {
    Owner owner{"world"};
    owner.bindToCurrentThread();
    std::vector<std::string> ran;

    std::thread{[&] {
        owner.submitConsole([&ran] { ran.emplace_back("console"); });
        owner.submit([&ran] { ran.emplace_back("task"); });
    }}.join();
    CHECK(ran.empty());
    // parity: `DedicatedServer.handleConsoleInputs`.
    CHECK(owner.runPending() == 1);
    CHECK(ran == std::vector<std::string>{"task"});
    CHECK(owner.runConsole() == 1);
    CHECK(ran == std::vector<std::string>{"task", "console"});
    CHECK(owner.pending() == 0);
}

TEST_CASE("a closed owner hands back a cancelled console handle") {
    Owner owner{"world"};
    owner.bindToCurrentThread();
    TaskHandle queued;
    std::thread{[&] { queued = owner.submitConsole([] {}); }}.join();
    owner.close();
    CHECK(queued.isCancelled());
    CHECK(owner.runConsole() == 0);
}

TEST_CASE("a merge carries console input over to the survivor") {
    Owner gone{"gone"};
    Owner survivor{"survivor"};
    gone.bindToCurrentThread();
    survivor.bindToCurrentThread();
    std::vector<std::string> ran;
    std::thread{[&] { gone.submitConsole([&ran] { ran.emplace_back("console"); }); }}.join();

    gone.drainInto(survivor);
    CHECK(gone.pending() == 0);
    CHECK(survivor.runPending() == 0);
    CHECK(ran.empty());
    CHECK(survivor.runConsole() == 1);
    CHECK(ran == std::vector<std::string>{"console"});
}

TEST_CASE("strict mode says who touched what, and off it says nothing") {
    Owner owner{"world"};
    {
        const StrictMode strict{true};
        CHECK_NOTHROW(owner.check("the chunk map"));
        owner.bindToCurrentThread();
        CHECK_NOTHROW(owner.check("the chunk map"));

        std::thread{[&owner] { CHECK_THROWS_AS(owner.check("the chunk map"), std::logic_error); }}.join();
    }
    {
        const StrictMode strict{false};
        std::thread{[&owner] { CHECK_NOTHROW(owner.check("the chunk map")); }}.join();
    }
    CHECK_FALSE(ownership::isStrict());
}

TEST_CASE("draining from a thread that does not own it is the caller's bug") {
    Owner owner{"world"};
    owner.bindToCurrentThread();
    // Not a strict-mode check: two threads draining one owner corrupt the queue whatever the flag says.
    std::thread{[&owner] {
        CHECK_THROWS_AS(owner.runPending(), std::logic_error);
        CHECK_THROWS_AS(owner.runEndOfTick(), std::logic_error);
    }}.join();

    Owner unclaimed{"unclaimed"};
    std::thread{[&unclaimed] { CHECK(unclaimed.runPending() == 0); }}.join();
}
