#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/timings/TimingsReport.hpp>

#include <cstdint>

namespace basalt::server::runtime::command::commands {

/// parity: `DebugCommand`, whose `start`/`stop` bracket a profiling run and write a report.
///
/// Vanilla's report is its own `ProfileResults` dump; this writes the same report `/timings report`
/// writes, because that is what this server measures with.
class DebugCommand final : public Command {
public:
    /// `runFunction` is what `/debug function` runs, or unset in a fixture with no functions.
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
    /// Mutable because `start` records it and `build` hands out const callbacks; the mark is the
    /// command's own state, not the tree's.
    mutable std::int64_t ticksAtStart_ = 0;
};

/// parity: `PerfCommand`, which in vanilla records a ten-second profile for a bug report. The same
/// measurements live behind `/timings` here, so this starts and stops that and says where the file
/// went rather than inventing a second profiler.
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

/// `/jfr`: kept so the command exists where an operator expects it, and answers instead of failing.
///
/// Java Flight Recorder is a JVM facility and this server does not run on one, which is the point
/// of the project rather than a gap in it. The command reports that and points at `/timings`, which
/// measures the same things. It stays registered so that whoever does implement a native recorder
/// has the command, its permission and its tree already in place.
class JfrCommand final : public Command {
public:
    explicit JfrCommand(const wire::common::Timings& timings);

    void build(const CommandNodeBuilder& root) const override;

private:
    const wire::common::Timings* timings_;
};

/// `/publish`: vanilla registers this only on the integrated server, where it opens a single-player
/// world to the local network. A dedicated server is already published, on the port `server.toml`
/// names, so the command answers with that rather than pretending to open a second one.
class PublishCommand final : public Command {
public:
    explicit PublishCommand(std::function<std::int32_t()> port);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<std::int32_t()> port_;
};

/// parity: `UnpublishCommand`, which closes a single-player world to the network. Nothing to close
/// here.
class UnpublishCommand final : public Command {
public:
    UnpublishCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `FetchProfileCommand`, which asks Mojang for the UUID behind a name.
///
/// `name` asks Mojang; `entity` and `id` answer from who is connected, which is the only profile
/// cache this build has.
class FetchProfileCommand final : public Command {
public:
    explicit FetchProfileCommand(std::function<std::optional<std::string>(const std::string& name)> lookup);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<std::optional<std::string>(const std::string&)> lookup_;
};

} // namespace basalt::server::runtime::command::commands
