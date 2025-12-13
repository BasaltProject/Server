#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/commands/StopwatchCommand.hpp>

#include <string>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::common::Identifier;
using wire::text::Component;

constexpr double kMillisPerSecond = 1000.0;

[[nodiscard]] CommandSyntaxError missing(const Identifier& id) {
    return CommandSyntaxError{
        .component = Component::translatable("commands.stopwatch.does_not_exist", {Component::text(id.toString())})};
}

} // namespace

std::vector<Identifier> Stopwatches::ids() const {
    std::vector<Identifier> named;
    named.reserve(started_.size());
    for (const auto& [id, start] : started_) {
        named.push_back(id);
    }
    return named;
}

bool Stopwatches::create(const Identifier& id) {
    if (started_.contains(id)) {
        return false;
    }
    started_.emplace(id, nowMillis_ ? nowMillis_() : 0);
    return true;
}

bool Stopwatches::restart(const Identifier& id) {
    const auto found = started_.find(id);
    if (found == started_.end()) {
        return false;
    }
    found->second = nowMillis_ ? nowMillis_() : 0;
    return true;
}

bool Stopwatches::remove(const Identifier& id) { return started_.erase(id) != 0; }

std::optional<double> Stopwatches::elapsedSeconds(const Identifier& id) const {
    const auto found = started_.find(id);
    if (found == started_.end()) {
        return std::nullopt;
    }
    return static_cast<double>((nowMillis_ ? nowMillis_() : 0) - found->second) / kMillisPerSecond;
}

StopwatchCommand::StopwatchCommand(Stopwatches& stopwatches)
    : Command("stopwatch", permissionLevel::kGamemasters), stopwatches_(&stopwatches) {}

void StopwatchCommand::build(const CommandNodeBuilder& root) const {
    root.literal("create", [this](const CommandNodeBuilder& form) {
        form.argument("id", resourceLocationArgument(), [this](const CommandNodeBuilder& id) {
            id.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto* named = context.get<Identifier>("id");
                if (named == nullptr) {
                    return 0;
                }
                if (!stopwatches_->create(*named)) {
                    return std::unexpected{
                        CommandSyntaxError{.component = Component::translatable("commands.stopwatch.already_exists",
                                                                                {Component::text(named->toString())})}};
                }
                context.success(
                    Component::translatable("commands.stopwatch.create.success", {Component::text(named->toString())}),
                    true);
                return 1;
            });
        });
    });

    /// parity: reports the seconds and returns them scaled, which is what `/execute store` reads.
    const auto query = [this](const CommandContext& context, double scale) -> Parsed<std::int32_t> {
        const auto* named = context.get<Identifier>("id");
        if (named == nullptr) {
            return 0;
        }
        const std::optional<double> seconds = stopwatches_->elapsedSeconds(*named);
        if (!seconds.has_value()) {
            return std::unexpected{missing(*named)};
        }
        context.success(
            Component::translatable("commands.stopwatch.query",
                                    {Component::text(named->toString()), Component::text(std::to_string(*seconds))}));
        return static_cast<std::int32_t>(*seconds * scale);
    };
    root.literal("query", [this, query](const CommandNodeBuilder& form) {
        form.argument("id", resourceLocationArgument(), [this, query](const CommandNodeBuilder& id) {
            id.suggestsFromServer(
                [this](const SuggestionContext& context) { return context.matchingIds(stopwatches_->ids()); });
            id.executes([query](const CommandContext& context) { return query(context, 1.0); });
            id.argument("scale", doubleArgument(), [query](const CommandNodeBuilder& scale) {
                scale.executes([query](const CommandContext& context) {
                    const auto* factor = context.get<double>("scale");
                    return query(context, factor == nullptr ? 1.0 : *factor);
                });
            });
        });
    });

    /// `restart` and `remove` differ only in which of the two they call and what they then say.
    const auto act = [this](bool removing) {
        return [this, removing](const CommandContext& context) -> Parsed<std::int32_t> {
            const auto* named = context.get<Identifier>("id");
            if (named == nullptr) {
                return 0;
            }
            if (!(removing ? stopwatches_->remove(*named) : stopwatches_->restart(*named))) {
                return std::unexpected{missing(*named)};
            }
            context.success(Component::translatable(removing ? "commands.stopwatch.remove.success"
                                                             : "commands.stopwatch.restart.success",
                                                    {Component::text(named->toString())}),
                            true);
            return 1;
        };
    };
    for (const bool removing : {false, true}) {
        root.literal(removing ? "remove" : "restart", [this, act, removing](const CommandNodeBuilder& form) {
            form.argument("id", resourceLocationArgument(), [this, act, removing](const CommandNodeBuilder& id) {
                id.suggestsFromServer(
                    [this](const SuggestionContext& context) { return context.matchingIds(stopwatches_->ids()); });
                id.executes(act(removing));
            });
        });
    }
}

} // namespace basalt::server::runtime::command::commands
