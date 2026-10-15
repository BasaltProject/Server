#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/timings/TimingsReport.hpp>

#include <cstdint>

namespace basalt::server::runtime::command::commands {

/// parity: `DebugCommand`.
class DebugCommand final : public Command {
public:
    DebugCommand(wire::common::Timings& timings, const timings::TimingsReport& report,
                 std::function<std::int64_t()> nowNanos, std::function<std::int64_t()> ticksRun,
                 std::function<std::optional<std::int32_t>(const CommandContext&, const wire::common::Identifier&)>
                     runFunction = {});

    void build(const CommandNodeBuilder& root) const override;

private:
    wire::common::Timings* timings_;
    const timings::TimingsReport* report_;
    std::function<std::int64_t()> nowNanos_;
    std::function<std::int64_t()> ticksRun_;
    std::function<std::optional<std::int32_t>(const CommandContext&, const wire::common::Identifier&)> runFunction_;
    /// Mutable because `build` hands out const callbacks.
    mutable std::int64_t ticksAtStart_ = 0;
};

/// parity: `PerfCommand`, which in vanilla records a ten-second profile for a bug report.
class PerfCommand final : public Command {
public:
    PerfCommand(wire::common::Timings& timings, const timings::TimingsReport& report,
                std::function<std::int64_t()> nowNanos);

    void build(const CommandNodeBuilder& root) const override;

private:
    wire::common::Timings* timings_;
    const timings::TimingsReport* report_;
    std::function<std::int64_t()> nowNanos_;
};

class JfrCommand final : public Command {
public:
    explicit JfrCommand(const wire::common::Timings& timings);

    void build(const CommandNodeBuilder& root) const override;

private:
    const wire::common::Timings* timings_;
};

class PublishCommand final : public Command {
public:
    explicit PublishCommand(std::function<std::int32_t()> port);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<std::int32_t()> port_;
};

/// parity: `UnpublishCommand`, which closes a single-player world to the network.
class UnpublishCommand final : public Command {
public:
    UnpublishCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `FetchProfileCommand`, which asks Mojang for the UUID behind a name.
class FetchProfileCommand final : public Command {
public:
    explicit FetchProfileCommand(std::function<std::optional<std::string>(const std::string& name)> lookup);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<std::optional<std::string>(const std::string&)> lookup_;
};

} // namespace basalt::server::runtime::command::commands
