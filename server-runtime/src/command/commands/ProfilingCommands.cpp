#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/ExecuteArguments.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/commands/ProfilingCommands.hpp>

#include <cmath>
#include <string>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::common::Identifier;
using wire::text::Component;
using wire::text::kGray;

/// Two decimals, which is what vanilla's own report prints.
[[nodiscard]] std::string formatted(double value) {
    const auto scaled = static_cast<std::int64_t>(std::llround(value * 100.0));
    const std::string fraction = std::to_string(scaled % 100);
    return std::to_string(scaled / 100) + "." + (fraction.size() < 2 ? "0" + fraction : fraction);
}

} // namespace

DebugCommand::DebugCommand(
    wire::common::Timings& timings, const timings::TimingsReport& report, std::function<std::int64_t()> nowNanos,
    std::function<std::int64_t()> ticksRun,
    std::function<std::optional<std::int32_t>(const CommandContext&, const Identifier&)> runFunction)
    : Command("debug", permissionLevel::kAdmins), timings_(&timings), report_(&report), nowNanos_(std::move(nowNanos)),
      ticksRun_(std::move(ticksRun)), runFunction_(std::move(runFunction)) {}

void DebugCommand::build(const CommandNodeBuilder& root) const {
    root.literal("start", [this](const CommandNodeBuilder& start) {
        start.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            if (timings_->enabled()) {
                return std::unexpected{
                    CommandSyntaxError{.component = Component::translatable("commands.debug.alreadyRunning")}};
            }
            timings_->reset(nowNanos_ ? nowNanos_() : 0);
            ticksAtStart_ = ticksRun_ ? ticksRun_() : 0;
            timings_->setEnabled(true);
            context.success(Component::translatable("commands.debug.started"), true);
            return 1;
        });
    });

    root.literal("stop", [this](const CommandNodeBuilder& stop) {
        stop.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            if (!timings_->enabled()) {
                return std::unexpected{
                    CommandSyntaxError{.component = Component::translatable("commands.debug.notRunning")}};
            }
            timings_->setEnabled(false);
            const std::int64_t now = nowNanos_ ? nowNanos_() : 0;
            const std::string path = report_->write(now);
            const double seconds = static_cast<double>(timings_->windowNanos(now)) / 1'000'000'000.0;
            const std::int64_t ticks = (ticksRun_ ? ticksRun_() : 0) - ticksAtStart_;
            context.success(
                Component::translatable(
                    "commands.debug.stopped",
                    {Component::text(formatted(seconds)), Component::text(std::to_string(ticks)),
                     Component::text(formatted(seconds <= 0.0 ? 0.0 : static_cast<double>(ticks) / seconds))}),
                true);
            context.source().sendSystemMessage(Component::text("  " + path, kGray));
            return 1;
        });
    });

    /// parity: `/debug function`, which runs a function and reports what it did. Vanilla writes a
    /// trace file per command; this reports the count, which is the part a player sees.
    root.literal("function", [this](const CommandNodeBuilder& form) {
        form.argument("name", functionArgument(), [this](const CommandNodeBuilder& name) {
            name.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                if (!runFunction_) {
                    return std::unexpected{
                        CommandSyntaxError{.component = Component::text("No functions are loaded.")}};
                }
                const auto* id = context.get<Identifier>("name");
                const std::optional<std::int32_t> result = id == nullptr ? std::nullopt : runFunction_(context, *id);
                if (!result.has_value()) {
                    return std::unexpected{
                        CommandSyntaxError{.component = Component::text("No functions are loaded.")}};
                }
                context.success(Component::translatable(
                    "commands.debug.function.success",
                    {Component::text("1"), Component::text(id->toString()), Component::text(std::to_string(*result))}));
                return *result;
            });
        });
    });
}

PerfCommand::PerfCommand(wire::common::Timings& timings, const timings::TimingsReport& report,
                         std::function<std::int64_t()> nowNanos)
    : Command("perf", permissionLevel::kAdmins), timings_(&timings), report_(&report), nowNanos_(std::move(nowNanos)) {}

void PerfCommand::build(const CommandNodeBuilder& root) const {
    root.literal("start", [this](const CommandNodeBuilder& start) {
        start.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            if (timings_->enabled()) {
                return std::unexpected{
                    CommandSyntaxError{.component = Component::translatable("commands.perf.alreadyRunning")}};
            }
            timings_->reset(nowNanos_ ? nowNanos_() : 0);
            timings_->setEnabled(true);
            context.success(Component::translatable("commands.perf.started"), true);
            return 1;
        });
    });
    root.literal("stop", [this](const CommandNodeBuilder& stop) {
        stop.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            if (!timings_->enabled()) {
                return std::unexpected{
                    CommandSyntaxError{.component = Component::translatable("commands.perf.notRunning")}};
            }
            timings_->setEnabled(false);
            const std::string path = report_->write(nowNanos_ ? nowNanos_() : 0);
            context.success(Component::translatable("commands.perf.reportSaved", {Component::text(path)}), true);
            context.source().sendSystemMessage(Component::text("  " + path, kGray));
            return 1;
        });
    });
}

JfrCommand::JfrCommand(const wire::common::Timings& timings)
    : Command("jfr", permissionLevel::kAdmins), timings_(&timings) {}

void JfrCommand::build(const CommandNodeBuilder& root) const {
    for (const std::string_view action : {"start", "stop"}) {
        root.literal(std::string{action}, [this](const CommandNodeBuilder& form) {
            form.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                context.success(Component::text("This server does not run on a JVM, so there is no Java Flight "
                                                "Recorder to start or stop."));
                context.source().sendSystemMessage(Component::text(
                    std::string{"  Use /timings for the same measurements"} +
                        (timings_->enabled() ? " (already recording)." : ", or /perf for a one-off report."),
                    kGray));
                // Zero rather than one: nothing was recorded, and `/execute store` should see that.
                return 0;
            });
        });
    }
}

PublishCommand::PublishCommand(std::function<std::int32_t()> port)
    : Command("publish", permissionLevel::kOwners), port_(std::move(port)) {}

void PublishCommand::build(const CommandNodeBuilder& root) const {
    const auto report = [this](const CommandContext& context) -> Parsed<std::int32_t> {
        const std::int32_t port = port_ ? port_() : 0;
        context.success(Component::text("This is a dedicated server: it is already accepting connections on port " +
                                        std::to_string(port) +
                                        ". /publish exists only on a single-player world being opened to a local "
                                        "network."));
        return port;
    };
    root.executes(report);
    root.argument("allowCommands", boolArgument(), [report](const CommandNodeBuilder& allow) {
        allow.executes(report);
        allow.argument("gamemode", gameModeArgument(), [report](const CommandNodeBuilder& mode) {
            mode.executes(report);
            mode.argument("port", integerArgument(0, 65535),
                          [report](const CommandNodeBuilder& port) { port.executes(report); });
        });
    });
}

UnpublishCommand::UnpublishCommand() : Command("unpublish", permissionLevel::kOwners) {}

void UnpublishCommand::build(const CommandNodeBuilder& root) const {
    root.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
        context.success(
            Component::text("This is a dedicated server: it is not published to a local network, so nothing closes."));
        return 1;
    });
}

FetchProfileCommand::FetchProfileCommand(std::function<std::optional<std::string>(const std::string&)> lookup)
    : Command("fetchprofile", permissionLevel::kAdmins), lookup_(std::move(lookup)) {}

void FetchProfileCommand::build(const CommandNodeBuilder& root) const {
    root.literal("name", [this](const CommandNodeBuilder& form) {
        form.argument("name", greedyStringArgument(), [this](const CommandNodeBuilder& named) {
            named.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto* name = context.get<std::string>("name");
                const std::optional<std::string> uuid = name == nullptr || !lookup_ ? std::nullopt : lookup_(*name);
                if (!uuid.has_value()) {
                    return std::unexpected{
                        CommandSyntaxError{.component = Component::text("No Mojang account is called '" +
                                                                        (name == nullptr ? std::string{} : *name) +
                                                                        "', or the lookup failed.")}};
                }
                context.success(Component::text(*name + " is " + *uuid));
                return 1;
            });
        });
    });
    root.literal("entity", [](const CommandNodeBuilder& form) {
        form.argument("entity", entitiesArgument(true), [](const CommandNodeBuilder& entity) {
            entity.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto* matched = context.get<std::vector<CommandTarget>>("entity");
                if (matched == nullptr || matched->size() != 1) {
                    return 0;
                }
                context.success(Component::text(matched->front().name() + " is " + matched->front().uuid()));
                return 1;
            });
        });
    });
    root.literal("id", [](const CommandNodeBuilder& form) {
        form.argument("id", uuidArgument(), [](const CommandNodeBuilder& id) {
            id.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto* uuid = context.get<std::string>("id");
                if (uuid == nullptr) {
                    return 0;
                }
                for (const player::PlayerSession* player : context.environment().players().snapshot()) {
                    if (player->uuid().toString() == *uuid) {
                        context.success(Component::text(*uuid + " is " + player->name()));
                        return 1;
                    }
                }
                return std::unexpected{
                    CommandSyntaxError{.component = Component::text("Nobody connected has the id '" + *uuid +
                                                                    "', and this build keeps no cache.")}};
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
