#include <basalt/server/runtime/command/commands/TimingsCommand.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::text::Component;
using wire::text::kGray;
using wire::text::kWhite;

[[nodiscard]] std::string decimals(double value, std::int32_t digits) {
    std::int64_t scale = 1;
    for (std::int32_t step = 0; step < digits; step++) {
        scale *= 10;
    }
    const auto rounded = static_cast<std::int64_t>(std::llround(value * static_cast<double>(scale)));
    const std::string fraction = std::to_string(rounded % scale);
    return std::to_string(rounded / scale) + "." +
           std::string(static_cast<std::size_t>(digits) - fraction.size(), '0') + fraction;
}

[[nodiscard]] std::string rate(double value) { return decimals(value, 2); }

/// Three decimals for durations, because the work being measured is often under a millisecond and
/// two decimals rendered a real 0.004ms tick as `0.00`, which reads as "no data".
[[nodiscard]] std::string millis(double value) { return decimals(value, 3); }

[[nodiscard]] std::string padded(const std::string& text, std::size_t width) {
    return text.size() >= width ? text : text + std::string(width - text.size(), ' ');
}

} // namespace

TimingsCommand::TimingsCommand(wire::common::Timings& timings, wire::common::TickDurations& tickDurations,
                               const timings::TimingsReport& report, std::function<std::int64_t()> nowNanos)
    : Command("timings", permissionLevel::kAdmins), timings_(&timings), tickDurations_(&tickDurations),
      report_(&report), nowNanos_(std::move(nowNanos)) {}

void TimingsCommand::build(const CommandNodeBuilder& root) const {
    /// The one screen worth reading in chat; everything else goes in the file.
    root.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
        const std::int64_t now = nowNanos_ ? nowNanos_() : 0;
        const CommandSource& source = context.source();
        source.sendSystemMessage(Component::text("Ticks per second:", kGray));
        source.sendSystemMessage(Component::text("  5s " + rate(tickDurations_->ticksPerSecond(now, 5)) + "   1m " +
                                                     rate(tickDurations_->ticksPerSecond(now, 60)) + "   5m " +
                                                     rate(tickDurations_->ticksPerSecond(now, 300)),
                                                 kWhite));
        source.sendSystemMessage(Component::text("Tick duration in ms (min/median/95%/max):", kGray));
        for (const int window : {10, 60, 300}) {
            const wire::common::DurationSummary summary = tickDurations_->summary(now, window);
            const std::string label = window < 60 ? std::to_string(window) + "s" : std::to_string(window / 60) + "m";
            source.sendSystemMessage(
                Component::text("  " + padded(label, 4) + " " + millis(summary.minMillis) + " / " +
                                    millis(summary.medianMillis) + " / " + millis(summary.percentile95Millis) + " / " +
                                    millis(summary.maxMillis) + "   (" + std::to_string(summary.samples) + " ticks)",
                                kWhite));
        }
        source.sendSystemMessage(Component::text(timings_->enabled()
                                                     ? "Measuring sections. /timings report writes them to a file."
                                                     : "Sections are not being measured. /timings start to begin.",
                                                 kGray));
        return 1;
    });

    root.literal("start", [this](const CommandNodeBuilder& start) {
        start.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            timings_->reset(nowNanos_ ? nowNanos_() : 0);
            timings_->setEnabled(true);
            context.success(Component::text("Measuring started. Run /timings report when you have seen the problem."),
                            true);
            return 1;
        });
    });

    root.literal("stop", [this](const CommandNodeBuilder& stop) {
        stop.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            timings_->setEnabled(false);
            context.success(Component::text("Measuring stopped. The numbers so far are kept."));
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

    root.literal("report", [this](const CommandNodeBuilder& report) {
        report.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            const std::string path = report_->write(nowNanos_ ? nowNanos_() : 0);
            context.success(Component::text("Timings written to " + path), true);
            const std::vector<wire::common::TimingSnapshot> sections = timings_->snapshot();
            if (std::ranges::none_of(sections, [](const wire::common::TimingSnapshot& one) { return one.calls > 0; })) {
                // Otherwise the file's empty `sections` reads as "nothing costs anything".
                context.source().sendSystemMessage(
                    Component::text("  Sections are empty: nothing measurable ran. /timings start, let the server "
                                    "do the thing you are investigating, then report.",
                                    kGray));
            }
            return 1;
        });
    });
}

} // namespace basalt::server::runtime::command::commands
