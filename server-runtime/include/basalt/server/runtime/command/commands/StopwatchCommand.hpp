#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace basalt::server::runtime::command::commands {

/// parity: `Stopwatches`.
class Stopwatches {
public:
    explicit Stopwatches(std::function<std::int64_t()> nowMillis) : nowMillis_(std::move(nowMillis)) {}

    [[nodiscard]] std::vector<wire::common::Identifier> ids() const;

    bool create(const wire::common::Identifier& id);

    bool restart(const wire::common::Identifier& id);

    bool remove(const wire::common::Identifier& id);

    [[nodiscard]] std::optional<double> elapsedSeconds(const wire::common::Identifier& id) const;

    /// parity: `Stopwatches.pack`, the milliseconds each one has run, under `stopwatches`.
    [[nodiscard]] wire::nbt::NbtCompound encode() const;

    /// parity: `Stopwatches.unpack`, each one resuming from what it had accumulated.
    void restore(const wire::nbt::NbtCompound& packed);

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
