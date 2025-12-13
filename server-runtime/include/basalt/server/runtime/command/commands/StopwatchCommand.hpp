#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <map>
#include <optional>
#include <vector>

namespace basalt::server::runtime::command::commands {

/// Named stopwatches, kept in memory and read back by id.
///
/// parity: `Stopwatches`, which counts wall-clock milliseconds rather than ticks, so a frozen or
/// lagging server does not change what one reads.
class Stopwatches {
public:
    explicit Stopwatches(std::function<std::int64_t()> nowMillis) : nowMillis_(std::move(nowMillis)) {}

    [[nodiscard]] std::vector<wire::common::Identifier> ids() const;

    bool create(const wire::common::Identifier& id);

    bool restart(const wire::common::Identifier& id);

    bool remove(const wire::common::Identifier& id);

    /// Seconds since it was created or last restarted, or nothing for one nobody made.
    [[nodiscard]] std::optional<double> elapsedSeconds(const wire::common::Identifier& id) const;

private:
    std::function<std::int64_t()> nowMillis_;
    std::map<wire::common::Identifier, std::int64_t> started_;
};

/// parity: `StopwatchCommand`, new in 26.2.
class StopwatchCommand final : public Command {
public:
    explicit StopwatchCommand(Stopwatches& stopwatches);

    void build(const CommandNodeBuilder& root) const override;

private:
    Stopwatches* stopwatches_;
};

} // namespace basalt::server::runtime::command::commands
