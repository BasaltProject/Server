#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/commands/TimeCommand.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::common::Identifier;
using wire::text::Component;

constexpr std::int32_t kDayTicks = 24000;
constexpr float kMinRate = 1.0E-5F;
constexpr float kMaxRate = 1000.0F;

struct Clock {
    world::LoadedWorld* world = nullptr;
    std::string name;
};

[[nodiscard]] std::vector<Identifier> clockIds() {
    return {Identifier::vanilla("overworld"), Identifier::vanilla("the_end")};
}

/// parity: `TimeCommand.wrapTime`.
[[nodiscard]] std::int32_t wrap(std::int64_t ticks) {
    return static_cast<std::int32_t>(ticks % std::numeric_limits<std::int32_t>::max());
}

[[nodiscard]] std::expected<Clock, CommandSyntaxError> clockOf(const CommandContext& context, bool named) {
    const Parsed<world::LoadedWorld*> world = context.world();
    if (!world.has_value()) {
        return std::unexpected{world.error()};
    }
    const config::WorldDimension dimension = (*world)->declaration().dimension;
    if (dimension == config::WorldDimension::Nether) {
        return std::unexpected{CommandSyntaxError{
            .component = Component::translatable("commands.time.no_default_clock",
                                                 {Component::text(config::dimensionRegistryKey(dimension))})}};
    }
    const std::string name = dimension == config::WorldDimension::End ? "minecraft:the_end" : "minecraft:overworld";
    if (named) {
        const auto* asked = context.get<Identifier>("clock");
        const std::string wanted = asked == nullptr ? std::string{} : asked->toString();
        if (wanted != name) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::text("This world runs on " + name + ", not " + wanted +
                                                                ": clocks belong to worlds here.")}};
        }
    }
    return Clock{.world = *world, .name = name};
}

/// False when a listener cancelled the change of the clock.
[[nodiscard]] bool timeAllowed(const CommandContext& context, const Clock& clock, std::int64_t to, const char* cause) {
    namespace events = server::plugins::pluginEventTypes;
    const server::plugins::PluginEvent* change = server::plugins::emit(
        context.environment().pluginEvents(), &events::worldTimeSkip(), [&](server::plugins::PluginEvent& event) {
            event.set("world", clock.name);
            event.set("time", to);
            event.set("from_time", clock.world->clock().totalTicks());
            event.set("cause", std::string{cause});
        });
    return change == nullptr || !change->cancelled();
}

[[nodiscard]] Parsed<std::int32_t> setAbsolute(const CommandContext& context, std::int64_t ticks, bool named) {
    const std::expected<Clock, CommandSyntaxError> clock = clockOf(context, named);
    if (!clock.has_value()) {
        return std::unexpected{clock.error()};
    }
    if (!timeAllowed(context, *clock, ticks, "command")) {
        return wrap(clock->world->clock().totalTicks());
    }
    clock->world->clock().setTotalTicks(ticks);
    context.environment().syncTime(*clock->world);
    context.success(Component::translatable("commands.time.set.absolute",
                                            {Component::text(clock->name), Component::text(std::to_string(ticks))}),
                    true);
    return wrap(ticks);
}

[[nodiscard]] Parsed<std::int32_t> setMarker(const CommandContext& context, bool named) {
    const std::expected<Clock, CommandSyntaxError> clock = clockOf(context, named);
    if (!clock.has_value()) {
        return std::unexpected{clock.error()};
    }
    const auto* marker = context.get<Identifier>("timemarker");
    const std::optional<std::int32_t> ticks =
        marker == nullptr ? std::nullopt : world::WorldClock::markerTicks(marker->path());
    if (!ticks.has_value()) {
        return std::unexpected{CommandSyntaxError{
            .component =
                Component::translatable("commands.time.set.time_marker.not_found",
                                        {Component::text(clock->name),
                                         Component::text(marker == nullptr ? std::string{} : marker->toString())})}};
    }
    clock->world->clock().setTotalTicks(
        world::WorldClock::timeOfNextMarker(clock->world->clock().totalTicks(), *ticks));
    context.environment().syncTime(*clock->world);
    context.success(Component::translatable("commands.time.set.time_marker",
                                            {Component::text(clock->name), Component::text(marker->toString())}),
                    true);
    return wrap(clock->world->clock().totalTicks());
}

[[nodiscard]] Parsed<std::int32_t> addTime(const CommandContext& context, bool named) {
    const std::expected<Clock, CommandSyntaxError> clock = clockOf(context, named);
    if (!clock.has_value()) {
        return std::unexpected{clock.error()};
    }
    const auto* ticks = context.get<std::int32_t>("time");
    const std::int32_t added = ticks == nullptr ? 0 : *ticks;
    if (!timeAllowed(context, *clock, clock->world->clock().totalTicks() + added, "command")) {
        return wrap(clock->world->clock().totalTicks());
    }
    clock->world->clock().addTicks(added);
    context.environment().syncTime(*clock->world);
    context.success(Component::translatable("commands.time.set.absolute",
                                            {Component::text(clock->name),
                                             Component::text(std::to_string(clock->world->clock().totalTicks()))}),
                    true);
    return wrap(clock->world->clock().totalTicks());
}

/// parity: `time query <timeline>`.
[[nodiscard]] Parsed<std::int32_t> queryTimeline(const CommandContext& context, bool named, bool repetitions) {
    const std::expected<Clock, CommandSyntaxError> clock = clockOf(context, named);
    if (!clock.has_value()) {
        return std::unexpected{clock.error()};
    }
    const auto* id = context.get<Identifier>("timeline");
    const world::RealTimelines* timelines = context.environment().options().timelines;
    const wire::nbt::NbtCompound* definition =
        timelines == nullptr || id == nullptr ? nullptr : timelines->definitionOf(*id);
    if (definition == nullptr) {
        return std::unexpected{CommandSyntaxError{
            .component = Component::translatable("commands.time.query.timeline.unknown",
                                                 {Component::text(id == nullptr ? std::string{} : id->toString())})}};
    }
    const std::string timelineClock{definition->getString("clock")};
    if (!timelineClock.empty() && timelineClock != clock->name) {
        return std::unexpected{CommandSyntaxError{
            .component =
                Component::text(id->toString() + " runs on " + timelineClock + ", not on " + clock->name + ".")}};
    }
    const std::int32_t declared = definition->getInt("period_ticks");
    const std::int64_t period = declared > 0 ? declared : kDayTicks;
    const std::int64_t ticks = clock->world->clock().totalTicks();
    const std::int64_t value = repetitions ? ticks / period : ticks % period;
    context.success(Component::translatable(repetitions ? "commands.time.query.timeline.repetitions"
                                                        : "commands.time.query.timeline",
                                            {Component::text(id->toString()), Component::text(std::to_string(value))}));
    return wrap(value);
}

void buildActions(const CommandNodeBuilder& root, bool named) {
    root.literal("set", [named](const CommandNodeBuilder& set) {
        set.argument("time", timeArgument(0), [named](const CommandNodeBuilder& time) {
            time.executes([named](const CommandContext& context) {
                const auto* ticks = context.get<std::int32_t>("time");
                return setAbsolute(context, ticks == nullptr ? 0 : *ticks, named);
            });
        });
        set.argument("timemarker", resourceLocationArgument(), [named](const CommandNodeBuilder& marker) {
            marker.suggestsFromServer([](const SuggestionContext& context) {
                std::vector<Identifier> markers;
                for (const auto& [name, ticks] : world::WorldClock::timeMarkers()) {
                    markers.push_back(Identifier::vanilla(std::string{name}));
                }
                return context.matchingIds(markers);
            });
            marker.executes([named](const CommandContext& context) { return setMarker(context, named); });
        });
    });
    root.literal("add", [named](const CommandNodeBuilder& add) {
        // parity: `TimeArgument.time(Integer.MIN_VALUE)`.
        add.argument("time", timeArgument(std::numeric_limits<std::int32_t>::min()),
                     [named](const CommandNodeBuilder& time) {
                         time.executes([named](const CommandContext& context) { return addTime(context, named); });
                     });
    });
    for (const bool pausing : {true, false}) {
        root.literal(pausing ? "pause" : "resume", [named, pausing](const CommandNodeBuilder& form) {
            form.executes([named, pausing](const CommandContext& context) -> Parsed<std::int32_t> {
                const std::expected<Clock, CommandSyntaxError> clock = clockOf(context, named);
                if (!clock.has_value()) {
                    return std::unexpected{clock.error()};
                }
                clock->world->clock().setPaused(pausing);
                context.environment().syncTime(*clock->world);
                context.success(Component::translatable(pausing ? "commands.time.pause" : "commands.time.resume",
                                                        {Component::text(clock->name)}),
                                true);
                return 1;
            });
        });
    }
    root.literal("rate", [named](const CommandNodeBuilder& form) {
        form.argument("rate", floatArgument(kMinRate, kMaxRate), [named](const CommandNodeBuilder& rate) {
            rate.executes([named](const CommandContext& context) -> Parsed<std::int32_t> {
                const std::expected<Clock, CommandSyntaxError> clock = clockOf(context, named);
                if (!clock.has_value()) {
                    return std::unexpected{clock.error()};
                }
                const auto* value = context.get<float>("rate");
                clock->world->clock().setRate(value == nullptr ? 1.0F : *value);
                context.environment().syncTime(*clock->world);
                context.success(
                    Component::translatable("commands.time.rate",
                                            {Component::text(clock->name),
                                             Component::text(std::to_string(value == nullptr ? 1.0F : *value))}),
                    true);
                return 1;
            });
        });
    });
    root.literal("query", [named](const CommandNodeBuilder& query) {
        query.literal("time", [named](const CommandNodeBuilder& time) {
            time.executes([named](const CommandContext& context) -> Parsed<std::int32_t> {
                const std::expected<Clock, CommandSyntaxError> clock = clockOf(context, named);
                if (!clock.has_value()) {
                    return std::unexpected{clock.error()};
                }
                const std::int64_t ticks = clock->world->clock().totalTicks();
                context.success(
                    Component::translatable("commands.time.query.absolute",
                                            {Component::text(clock->name), Component::text(std::to_string(ticks))}));
                return wrap(ticks);
            });
        });
        if (!named) {
            query.literal("gametime", [](const CommandNodeBuilder& gametime) {
                gametime.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                    const Parsed<world::LoadedWorld*> world = context.world();
                    if (!world.has_value()) {
                        return std::unexpected{world.error()};
                    }
                    const std::int64_t gameTime = (*world)->gameTime();
                    context.success(Component::translatable("commands.time.query.gametime",
                                                            {Component::text(std::to_string(gameTime))}));
                    return wrap(gameTime);
                });
            });
            /// Not vanilla's: kept from the old `/time query day`, which operators still reach for.
            query.literal("day", [](const CommandNodeBuilder& day) {
                day.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                    const Parsed<world::LoadedWorld*> world = context.world();
                    if (!world.has_value()) {
                        return std::unexpected{world.error()};
                    }
                    const std::int64_t days = (*world)->clock().totalTicks() / world::WorldClock::kDayLength;
                    context.success(Component::text("Day " + std::to_string(days)));
                    return wrap(days);
                });
            });
        }
        query.argument("timeline", resourceArgument("minecraft:timeline"), [named](const CommandNodeBuilder& timeline) {
            timeline.suggestsFromServer([](const SuggestionContext& context) {
                const world::RealTimelines* timelines = context.environment().options().timelines;
                return context.matchingIds(timelines == nullptr ? std::vector<Identifier>{} : timelines->ids());
            });
            timeline.executes([named](const CommandContext& context) { return queryTimeline(context, named, false); });
            timeline.literal("repetition", [named](const CommandNodeBuilder& repetition) {
                repetition.executes(
                    [named](const CommandContext& context) { return queryTimeline(context, named, true); });
            });
        });
    });
}

} // namespace

TimeCommand::TimeCommand() : Command("time", permissionLevel::kGamemasters) {}

void TimeCommand::build(const CommandNodeBuilder& root) const {
    buildActions(root, false);
    root.literal("of", [](const CommandNodeBuilder& of) {
        of.argument("clock", resourceArgument("minecraft:world_clock"), [](const CommandNodeBuilder& clock) {
            clock.suggestsFromServer([](const SuggestionContext& context) { return context.matchingIds(clockIds()); });
            buildActions(clock, true);
        });
    });
}

} // namespace basalt::server::runtime::command::commands
