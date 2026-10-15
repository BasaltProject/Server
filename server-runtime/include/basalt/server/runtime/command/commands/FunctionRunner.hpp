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

/// parity: `ServerFunctionManager`.
class FunctionRunner {
public:
    struct Settings {
        /// parity: `function-permission-level`, what a function's lines parse and run with.
        std::int32_t permissionLevel = 2;
        /// parity: the `max_command_sequence_length` rule of the world a call starts in.
        std::function<std::int32_t(const CommandSource& source)> commandLimit;
    };

    struct Returned {
        bool success = true;
        std::int32_t value = 0;
    };

    FunctionRunner(const function::FunctionLibrary& library, Settings settings)
        : library_(&library), settings_(std::move(settings)) {}

    [[nodiscard]] const function::FunctionLibrary& library() const noexcept { return *library_; }

    [[nodiscard]] std::int32_t permissionLevel() const noexcept { return settings_.permissionLevel; }

    void requestReturn(Returned value) { pendingReturn_ = value; }

    [[nodiscard]] bool insideFunction() const noexcept { return !frames_.empty(); }

    [[nodiscard]] Parsed<std::optional<Returned>> call(const CommandDispatcher& dispatcher, const CommandSource& source,
                                                       const function::ServerFunction& function,
                                                       const wire::nbt::NbtCompound* arguments = nullptr);

    using Done = std::function<void(const std::optional<Returned>& returned)>;

    Parsed<void> defer(const CommandDispatcher& dispatcher, const CommandSource& source,
                       const function::ServerFunction& function, const wire::nbt::NbtCompound* arguments,
                       Done onDone = {});

    /// parity: `IsolatedCall`.
    void deferUntilReturn(const CommandDispatcher& dispatcher, std::shared_ptr<const CommandSource> source,
                          std::vector<const function::ServerFunction*> functions, Done onDone);

    [[nodiscard]] std::shared_ptr<const CommandSource> keep(const CommandSource& source) const;

    [[nodiscard]] Parsed<std::int32_t> run(const CommandDispatcher& dispatcher, const CommandSource& source,
                                           const wire::common::Identifier& id,
                                           const wire::nbt::NbtCompound* arguments = nullptr);

    /// parity: `ServerFunctionManager.execute` of every function in a tag.
    void runTag(const CommandDispatcher& dispatcher, const CommandSource& source, const wire::common::Identifier& tag);

    /// parity: `ServerFunctionManager.tick`.
    void tick(const CommandDispatcher& dispatcher, const CommandSource& gameLoop);

    /// parity: `postReload`.
    void loaded() { runLoad_ = true; }

private:
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

/// parity: `TimerQueue`.
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

    [[nodiscard]] std::vector<std::string> names() const;

    [[nodiscard]] std::vector<Event> due(std::int64_t gameTime);

    /// parity: the `scheduled_events` saved data, `{events: [{trigger_time, id, callback}]}`.
    [[nodiscard]] wire::nbt::NbtCompound toNbt() const;

    void readNbt(const wire::nbt::NbtCompound& saved);

private:
    std::vector<Event> events_;
    std::uint64_t sequence_ = 0;
};

} // namespace basalt::server::runtime::command::commands
