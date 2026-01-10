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

/// Restores strict mode however the test leaves it, since it is process-wide.
struct StrictMode {
    explicit StrictMode(bool strict) : previous_(ownership::isStrict()) { ownership::setStrict(strict); }

    StrictMode(const StrictMode&) = delete;
    StrictMode& operator=(const StrictMode&) = delete;

    ~StrictMode() { ownership::setStrict(previous_); }

private:
    bool previous_;
};

/// A failure handler that keeps what it was told rather than rethrowing it.
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

    // Nobody owns it yet, so whoever is holding it is the owner: a world built by a test, or one
    // that exists before the first tick.
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

    // Submitted from the owning thread while draining, so it runs inline rather than queueing: a
    // chain of gameplay does not wait a tick for its own next step.
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

    // One still turns away when it is cancelled is dropped where it waits, rather than counting
    // down for turns nobody is going to use it for.
    const TaskHandle waiting = owner.schedule(20, [&ran] { ++ran; });
    CHECK(owner.pending() == 1);
    waiting.cancel();
    owner.advanceTick();
    CHECK(owner.pending() == 0);

    const TaskHandle atEnd = owner.scheduleAtEnd([&ran] { ++ran; });
    atEnd.cancel();
    CHECK(owner.runEndOfTick() == 0);
    CHECK(ran == 0);

    // A copy of a handle is the same handle, which is what makes handing one around safe.
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
    // A negative delay is not a task from the past: it is due next turn, like zero.
    owner.schedule(-5, [&ran] { ran.emplace_back("negative"); });
    CHECK(owner.pending() == 3);

    // Nothing runs until the turn it was scheduled for, and nothing runs without a turn at all.
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
    // The two queues are separate: draining one does not drain the other.
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

    // Without a handler the throw is the caller's, which is what a test wants to see.
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
    // What was waiting is cancelled rather than run, and the handles say so.
    CHECK(queued.isCancelled());
    CHECK(delayed.isCancelled());
    CHECK(atEnd.isCancelled());
    CHECK(owner.pending() == 0);
    CHECK(owner.runPending() == 0);
    CHECK(owner.runEndOfTick() == 0);
    CHECK(ran == 0);

    // And nothing new is taken, rather than being held for ever.
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

    // Queued from another thread, because an owner that is current runs its work on the spot.
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
    // The cancelled one is not carried over, and the delayed one keeps its remaining turns.
    CHECK(survivor.pending() == 3);

    CHECK(survivor.runPending() == 2);
    CHECK(ran == std::vector<std::string>{"queued", "at end"});
    // Three turns to go and then the turn it runs on, which is the same count it had before the
    // move: a merge is not a reason for a task to come due sooner or later than it asked.
    for (std::int32_t turn = 0; turn < 4; ++turn) {
        survivor.advanceTick();
        survivor.runPending();
    }
    CHECK(ran == std::vector<std::string>{"queued", "at end", "delayed"});

    // Draining an owner with nothing on it is not an error and leaves the other alone.
    Owner empty{"empty"};
    empty.drainInto(survivor);
    CHECK(survivor.pending() == 0);
}

TEST_CASE("strict mode says who touched what, and off it says nothing") {
    Owner owner{"world"};
    {
        const StrictMode strict{true};
        // Nobody has claimed it, so there is nothing to enforce yet.
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
    // Not a strict-mode check: two threads draining the same owner corrupt the queue whatever the
    // flag says, so this one is always on.
    std::thread{[&owner] {
        CHECK_THROWS_AS(owner.runPending(), std::logic_error);
        CHECK_THROWS_AS(owner.runEndOfTick(), std::logic_error);
    }}.join();

    // An owner nobody has claimed is drainable by whoever is holding it.
    Owner unclaimed{"unclaimed"};
    std::thread{[&unclaimed] { CHECK(unclaimed.runPending() == 0); }}.join();
}
