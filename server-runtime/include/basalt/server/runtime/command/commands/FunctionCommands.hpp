#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandStorage.hpp>
#include <basalt/server/runtime/function/FunctionLibrary.hpp>

#include <vector>

namespace basalt::server::runtime::command::commands {

/// Runs a function's lines, and holds what `/return` did while they run.
///
/// parity: `ServerFunctionManager`, minus its command-chain limits: `maxCommandChainLength` caps
/// the whole cascade in vanilla, and here the depth cap is what catches the failure that actually
/// matters, a function that calls itself.
class FunctionRunner {
public:
    static constexpr std::int32_t kMaxDepth = 64;

    explicit FunctionRunner(const function::FunctionLibrary& library) : library_(&library) {}

    /// Set by `/return` while a function is running, and cleared by the runner that reads it.
    void requestReturn(std::int32_t value) { pendingReturn_ = value; }

    [[nodiscard]] bool insideFunction() const noexcept { return depth_ > 0; }

    [[nodiscard]] Parsed<std::int32_t> run(const CommandDispatcher& dispatcher, const CommandSource& source,
                                           const wire::common::Identifier& id,
                                           const wire::nbt::NbtCompound* arguments = nullptr);

private:
    /// parity: a macro line starts with `$` and takes `$(name)` from the arguments.
    ///
    /// A line with no `$` prefix is left alone, which is what makes a macro function ordinary lines
    /// plus a few substituted ones.
    [[nodiscard]] Parsed<std::string> expand(const std::string& line, const wire::nbt::NbtCompound* arguments) const;

    const function::FunctionLibrary* library_;
    std::int32_t depth_ = 0;
    std::optional<std::int32_t> pendingReturn_;
};

/// Functions waiting for a tick to come round. parity: `TimerQueue`, keyed by function id.
class FunctionSchedule {
public:
    /// Answers the tick it will run on.
    std::int64_t schedule(const wire::common::Identifier& id, std::int32_t delayTicks, bool replace);

    /// What is waiting right now, which is what `/schedule clear` completes with.
    [[nodiscard]] std::vector<wire::common::Identifier> scheduledIds() const;

    std::int32_t clear(const wire::common::Identifier& id);

    /// Advances one tick and answers whatever is due now, in the order it was scheduled.
    [[nodiscard]] std::vector<wire::common::Identifier> tick();

private:
    struct Entry {
        wire::common::Identifier id;
        std::int64_t runAtTick = 0;
    };

    std::vector<Entry> entries_;
    std::int64_t tick_ = 0;
};

/// parity: `FunctionCommand`, macro arguments included.
class FunctionCommand final : public Command {
public:
    /// `data` is where `with block|entity|storage` reads its arguments, or null in a fixture
    /// without any.
    FunctionCommand(const function::FunctionLibrary& library, FunctionRunner& runner, const DataAccess* data = nullptr);

    void build(const CommandNodeBuilder& root) const override;

private:
    const function::FunctionLibrary* library_;
    FunctionRunner* runner_;
    const DataAccess* data_;
};

/// parity: `ReturnCommand`. Only meaningful inside a function, which is why it refuses outside one
/// rather than pretending to have returned from the console.
class ReturnCommand final : public Command {
public:
    explicit ReturnCommand(FunctionRunner& runner);

    void build(const CommandNodeBuilder& root) const override;

private:
    FunctionRunner* runner_;
};

/// parity: `ScheduleCommand`. The queue is ticked by the server, so a scheduled function runs on
/// the tick it was asked for rather than after a wall-clock delay.
class ScheduleCommand final : public Command {
public:
    ScheduleCommand(const function::FunctionLibrary& library, FunctionSchedule& schedule);

    void build(const CommandNodeBuilder& root) const override;

private:
    const function::FunctionLibrary* library_;
    FunctionSchedule* schedule_;
};

/// parity: `DataPackCommand`, over the packs the function library found.
class DataPackCommand final : public Command {
public:
    explicit DataPackCommand(function::FunctionLibrary& library);

    void build(const CommandNodeBuilder& root) const override;

private:
    function::FunctionLibrary* library_;
};

} // namespace basalt::server::runtime::command::commands
