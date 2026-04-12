#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/function/FunctionLibrary.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {

/// Runs functions, and holds what `/return` did while they run. parity: `ServerFunctionManager`
/// with the execution context `Commands.executeCommandInContext` opens. A nested call is queued as in
/// vanilla, so only `max_command_sequence_length` bounds recursion; whatever waits on its result
/// (`execute store`, `return run`, `execute if function`) is told when it returns.
class FunctionRunner {
public:
    struct Settings {
        /// parity: `function-permission-level`, what a function's lines parse and run with.
        std::int32_t permissionLevel = 2;
        /// parity: the `max_command_sequence_length` rule of the world a call starts in.
        std::function<std::int32_t(const CommandSource& source)> commandLimit;
    };

    /// A function's answer: what `/return` gave, and whether it succeeded, or nothing when it ran
    /// to the end without returning.
    struct Returned {
        bool success = true;
        std::int32_t value = 0;
    };

    FunctionRunner(const function::FunctionLibrary& library, Settings settings)
        : library_(&library), settings_(std::move(settings)) {}

    [[nodiscard]] const function::FunctionLibrary& library() const noexcept { return *library_; }

    [[nodiscard]] std::int32_t permissionLevel() const noexcept { return settings_.permissionLevel; }

    /// Set by `/return` while a function is running, and cleared by the runner that reads it.
    void requestReturn(Returned value) { pendingReturn_ = value; }

    [[nodiscard]] bool insideFunction() const noexcept { return !frames_.empty(); }

    /// Runs one function as `source`, macro arguments filled in first. An error is an instantiation
    /// failure, already worded as vanilla words it; nothing has run then.
    [[nodiscard]] Parsed<std::optional<Returned>> call(const CommandDispatcher& dispatcher, const CommandSource& source,
                                                       const function::ServerFunction& function,
                                                       const wire::nbt::NbtCompound* arguments = nullptr);

    /// Told when a queued function is done: its return, or nothing when it ran to the end.
    using Done = std::function<void(const std::optional<Returned>& returned)>;

    /// Queues a function behind the command running now. Only inside a function; the error is an
    /// instantiation failure, and nothing is queued then.
    Parsed<void> defer(const CommandDispatcher& dispatcher, const CommandSource& source,
                       const function::ServerFunction& function, const wire::nbt::NbtCompound* arguments,
                       Done onDone = {});

    /// parity: `IsolatedCall`: the functions one after another until one returns, then `onDone`.
    void deferUntilReturn(const CommandDispatcher& dispatcher, std::shared_ptr<const CommandSource> source,
                          std::vector<const function::ServerFunction*> functions, Done onDone);

    /// A copy of a source that outlives the command it came from, for work queued behind it.
    [[nodiscard]] std::shared_ptr<const CommandSource> keep(const CommandSource& source) const;

    /// A function by id, answering its return value or 0. What `/test` and `/debug function` use.
    [[nodiscard]] Parsed<std::int32_t> run(const CommandDispatcher& dispatcher, const CommandSource& source,
                                           const wire::common::Identifier& id,
                                           const wire::nbt::NbtCompound* arguments = nullptr);

    /// parity: `ServerFunctionManager.execute` of every function in a tag, each as its own call.
    void runTag(const CommandDispatcher& dispatcher, const CommandSource& source, const wire::common::Identifier& tag);

    /// parity: `ServerFunctionManager.tick`: `#minecraft:load` once after a load, then `#minecraft:tick`.
    void tick(const CommandDispatcher& dispatcher, const CommandSource& gameLoop);

    /// Whether the next tick runs `#minecraft:load` first. parity: `postReload`.
    void loaded() { runLoad_ = true; }

private:
    /// The lines to run, macros filled in and checked, or why the function cannot run.
    [[nodiscard]] Parsed<std::vector<std::string>> instantiate(const CommandDispatcher& dispatcher,
                                                               const CommandSource& source,
                                                               const function::ServerFunction& function,
                                                               const wire::nbt::NbtCompound* arguments) const;

    struct Frame {
        const CommandSource* source = nullptr;
        std::shared_ptr<const CommandSource> owned;
        std::vector<std::string> lines;
        std::size_t next = 0;
        Done onDone;
        std::optional<Returned> returned;
    };

    std::optional<Returned> runFrames(const CommandDispatcher& dispatcher, std::size_t base);

    void settle(const CommandDispatcher& dispatcher, Frame& finished);

    /// What the command that just ran queued goes on top, first queued first run.
    void afterCommand();

    const function::FunctionLibrary* library_;
    Settings settings_;
    std::vector<Frame> frames_;
    std::vector<Frame> deferred_;
    std::optional<Returned> pendingReturn_;
    std::int32_t quota_ = 0;
    std::int32_t limit_ = 0;
    bool exhausted_ = false;
    bool runLoad_ = true;
};

/// Functions waiting for a game time to come round. parity: `TimerQueue`, keyed by the name
/// `/schedule` gives: a function's id, or `#` and a tag's.
class FunctionSchedule {
public:
    struct Event {
        std::string name;
        wire::common::Identifier id;
        bool isTag = false;
        std::int64_t triggerTime = 0;
        std::uint64_t sequence = 0;
    };

    void schedule(const wire::common::Identifier& id, bool isTag, std::int64_t triggerTime);

    /// parity: `TimerQueue.remove`, answering how many went.
    std::int32_t remove(std::string_view name);

    /// What is waiting right now, which is what `/schedule clear` completes with.
    [[nodiscard]] std::vector<std::string> names() const;

    /// Everything due by `gameTime`, in trigger order and then the order it was scheduled.
    [[nodiscard]] std::vector<Event> due(std::int64_t gameTime);

    /// parity: the `scheduled_events` saved data, `{events: [{trigger_time, id, callback}]}`.
    [[nodiscard]] wire::nbt::NbtCompound toNbt() const;

    /// Replaces what is waiting with what was saved, skipping any event it cannot read.
    void readNbt(const wire::nbt::NbtCompound& saved);

private:
    std::vector<Event> events_;
    std::uint64_t sequence_ = 0;
};

} // namespace basalt::server::runtime::command::commands
