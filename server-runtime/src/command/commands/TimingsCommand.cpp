#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/commands/TimingsCommand.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/timings/TimingsUpload.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/text/ClickEvent.hpp>

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::text::ClickEvent;
using wire::text::Component;
using wire::text::kAqua;
using wire::text::kGray;
using wire::text::kGreen;
using wire::text::kRed;
using wire::text::kWhite;
using wire::text::kYellow;

constexpr std::int32_t kMaxTimeoutSeconds = 24 * 60 * 60;

[[nodiscard]] std::string decimals(double value, std::int32_t digits) {
    std::int64_t scale = 1;
    for (std::int32_t step = 0; step < digits; step++) {
        scale *= 10;
    }
    const auto rounded = static_cast<std::int64_t>(std::llround(value * static_cast<double>(scale)));
    const std::string fraction = std::to_string(std::abs(rounded % scale));
    return std::string{rounded < 0 ? "-" : ""} + std::to_string(std::abs(rounded / scale)) + "." +
           std::string(static_cast<std::size_t>(digits) - fraction.size(), '0') + fraction;
}

[[nodiscard]] Component line(std::vector<Component> parts) { return Component::join(Component::empty(), parts); }

[[nodiscard]] Component tpsOf(double value) {
    const auto color = value >= 19.0 ? kGreen : (value >= 15.0 ? kYellow : kRed);
    return Component::text(decimals(value, 1) + (value >= 20.0 ? "*" : ""), color);
}

/// Green while a tick fits comfortably in its fifty milliseconds, red once it no longer does.
[[nodiscard]] Component millisOf(double value) {
    const auto color = value <= 35.0 ? kGreen : (value <= 50.0 ? kYellow : kRed);
    return Component::text(decimals(value, 1), color);
}

[[nodiscard]] std::string spanOf(std::int64_t seconds) {
    if (seconds < 60) {
        return std::to_string(seconds) + "s";
    }
    if (seconds < 3600) {
        return std::to_string(seconds / 60) + "m " + std::to_string(seconds % 60) + "s";
    }
    return std::to_string(seconds / 3600) + "h " + std::to_string(seconds % 3600 / 60) + "m";
}

[[nodiscard]] Component linkTo(const std::string& url) {
    return Component::text(url, kAqua).underlined().clickEvent(ClickEvent::openUrl(url));
}

} // namespace

TimingsCommand::TimingsCommand(wire::common::Timings& timings, wire::common::TickDurations& tickDurations,
                               const timings::TimingsReport& report, std::function<std::int64_t()> nowNanos,
                               TimingsHooks hooks)
    : Command("timings", permissionLevel::kAdmins), timings_(&timings), tickDurations_(&tickDurations),
      report_(&report), nowNanos_(std::move(nowNanos)), hooks_(std::move(hooks)) {}

TimingsCommand::~TimingsCommand() {
    timer_.request_stop();
    const std::scoped_lock lock{workersMutex_};
    workers_.clear();
}

std::string TimingsCommand::requesterOf(const CommandContext& context) {
    const player::PlayerSession* player = context.source().player();
    return player == nullptr ? std::string{} : player->uuid().toString();
}

void TimingsCommand::start(const CommandContext& context, std::int32_t timeoutSeconds) const {
    const std::int64_t now = nowNanos_ ? nowNanos_() : 0;
    const bool restarted = timings_->enabled();
    timings_->reset(now);
    timings_->setEnabled(true);

    timer_.request_stop();
    if (timeoutSeconds > 0) {
        const std::string requester = requesterOf(context);
        timer_ = std::jthread{[this, requester, timeoutSeconds](const std::stop_token& stop) {
            std::mutex gate;
            std::condition_variable_any wake;
            std::unique_lock lock{gate};
            if (wake.wait_for(lock, stop, std::chrono::seconds{timeoutSeconds}, [] { return false; }) ||
                stop.stop_requested() || !hooks_.postToTick) {
                return;
            }
            hooks_.postToTick([this, requester] { finish(requester, true); });
        }};
    }

    std::vector<Component> parts{Component::text(restarted ? "Profiler restarted" : "Profiler started", kGreen)};
    if (timeoutSeconds > 0) {
        parts.push_back(Component::text(", it stops by itself in " + spanOf(timeoutSeconds), kGray));
    }
    parts.push_back(Component::text(". ", kGray));
    parts.push_back(Component::text("/timings stop", kWhite).clickEvent(ClickEvent::suggestCommand("/timings stop")));
    parts.push_back(Component::text(" ends it and gives you a link.", kGray));
    context.success(line(std::move(parts)), true);
}

void TimingsCommand::finish(const std::string& requester, bool stopMeasuring) const {
    const std::int64_t now = nowNanos_ ? nowNanos_() : 0;
    if (stopMeasuring) {
        timings_->setEnabled(false);
        timer_.request_stop();
    }
    const std::string json = report_->json(now);
    const std::string path = report_->save(json);
    const std::int64_t seconds = timings_->windowNanos(now) / wire::common::TickDurations::kNanosPerSecond;
    const timings::TimingsReport& report = *report_;
    const auto tell = [this, requester](const Component& message) {
        if (hooks_.tell) {
            hooks_.tell(requester, message);
        }
    };

    std::vector<Component> opening{Component::text("Profiled " + spanOf(seconds), kGreen)};
    if (!report.hasProfile()) {
        opening.push_back(Component::text(", but nothing measurable ran. Start it, let the server do the thing you "
                                          "are investigating, then stop.",
                                          kYellow));
    }
    tell(line(std::move(opening)));

    if (hooks_.viewerUrl.empty() || !hooks_.postToTick) {
        tell(line({Component::text("Saved to ", kGray), Component::text(path, kWhite)}));
        return;
    }
    tell(Component::text("Uploading the report...", kGray));
    const std::scoped_lock lock{workersMutex_};
    workers_.emplace_back([this, requester, json, path] {
        const std::string trust =
            hooks_.caFile.empty() ? std::string{} : wire::io::fileIO::readText(hooks_.caFile).value_or("");
        const auto uploaded = timings::uploadReport(hooks_.viewerUrl, json, trust);
        hooks_.postToTick([this, requester, uploaded, path] {
            if (!hooks_.tell) {
                return;
            }
            if (uploaded.has_value()) {
                hooks_.tell(requester, line({Component::text("Report: ", kGreen), linkTo(uploaded->url)}));
            } else {
                hooks_.tell(requester,
                            line({Component::text("Could not upload (" + uploaded.error() + "), saved to ", kRed),
                                  Component::text(path, kWhite)}));
                return;
            }
            hooks_.tell(requester, Component::text("Also saved to " + path, kGray));
        });
    });
}

void TimingsCommand::build(const CommandNodeBuilder& root) const {
    root.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
        const std::int64_t now = nowNanos_ ? nowNanos_() : 0;
        const CommandSource& source = context.source();
        const auto rate = [&](std::int32_t window) { return tpsOf(tickDurations_->ticksPerSecond(now, window)); };
        source.sendSystemMessage(Component::text("Basalt timings", kWhite).bold());
        source.sendSystemMessage(line({Component::text("TPS 5s/1m/5m: ", kGray), rate(5), Component::text(", ", kGray),
                                       rate(60), Component::text(", ", kGray), rate(300)}));
        const wire::common::DurationSummary ten = tickDurations_->summary(now, 10);
        const wire::common::DurationSummary minute = tickDurations_->summary(now, 60);
        source.sendSystemMessage(
            line({Component::text("MSPT 10s/1m (mean, 95%, max): ", kGray), millisOf(ten.meanMillis),
                  Component::text("/", kGray), millisOf(ten.percentile95Millis), Component::text("/", kGray),
                  millisOf(ten.maxMillis), Component::text("  ", kGray), millisOf(minute.meanMillis),
                  Component::text("/", kGray), millisOf(minute.percentile95Millis), Component::text("/", kGray),
                  millisOf(minute.maxMillis)}));
        if (timings_->enabled()) {
            source.sendSystemMessage(Component::text(
                "Profiling for " + spanOf(timings_->windowNanos(now) / wire::common::TickDurations::kNanosPerSecond) +
                    ". /timings stop ends it and gives you a link.",
                kGreen));
        } else {
            source.sendSystemMessage(Component::text(
                "Not profiling. /timings start [timeout <seconds>], then /timings stop for a link.", kGray));
        }
        return 1;
    });

    root.literal("start", [this](const CommandNodeBuilder& begin) {
        begin.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            start(context, 0);
            return 1;
        });
        begin.literal("timeout", [this](const CommandNodeBuilder& timeout) {
            timeout.argument("seconds", integerArgument(1, kMaxTimeoutSeconds),
                             [this](const CommandNodeBuilder& seconds) {
                                 seconds.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                                     const auto* value = context.get<std::int32_t>("seconds");
                                     start(context, value == nullptr ? 0 : *value);
                                     return 1;
                                 });
                             });
        });
    });

    root.literal("stop", [this](const CommandNodeBuilder& stop) {
        stop.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            if (!timings_->enabled()) {
                return std::unexpected{CommandSyntaxError{
                    .component = Component::text("The profiler is not running. /timings start begins it.")}};
            }
            finish(requesterOf(context), true);
            return 1;
        });
    });

    root.literal("report", [this](const CommandNodeBuilder& report) {
        report.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            finish(requesterOf(context), false);
            return 1;
        });
    });

    root.literal("status", [this](const CommandNodeBuilder& status) {
        status.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            const std::int64_t now = nowNanos_ ? nowNanos_() : 0;
            if (!timings_->enabled()) {
                context.success(Component::text("The profiler is not running.", kGray));
                return 0;
            }
            const std::int64_t seconds = timings_->windowNanos(now) / wire::common::TickDurations::kNanosPerSecond;
            context.success(Component::text("The profiler has been running for " + spanOf(seconds) + ".", kGreen));
            return 1;
        });
    });

    root.literal("reset", [this](const CommandNodeBuilder& reset) {
        reset.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            timings_->reset(nowNanos_ ? nowNanos_() : 0);
            tickDurations_->reset();
            context.success(Component::text("Counters cleared."));
            return 1;
        });
    });
}

} // namespace basalt::server::runtime::command::commands
