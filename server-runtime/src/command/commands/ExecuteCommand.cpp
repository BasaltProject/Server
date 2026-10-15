#include "ExecuteCommand.hpp"

#include <basalt/server/runtime/command/ArgumentTypes.hpp>

#include <cmath>
#include <cstdint>

namespace basalt::server::runtime::command::commands {
namespace executeCommand {
namespace {

using wire::text::Component;

/// parity: the six number types vanilla writes with `/execute store`.
constexpr std::array<std::string_view, 6> kNumberTypes{"byte", "double", "float", "int", "long", "short"};

[[nodiscard]] std::int32_t counted(const Test& test, const CommandContext& context, bool positive) {
    const std::optional<std::int32_t> matched = test(context);
    if (matched.has_value() != positive) {
        return 0;
    }
    return matched.value_or(1);
}

} // namespace

const std::vector<CommandTarget>& targetsOf(const CommandContext& context, std::string_view name) {
    static const std::vector<CommandTarget> none;
    const auto* found = context.get<std::vector<CommandTarget>>(name);
    return found == nullptr ? none : *found;
}

std::array<std::int32_t, 3> blockPosOf(const CommandContext& context, std::string_view name) {
    const auto* coordinates = context.get<BlockCoordinates>(name);
    return coordinates == nullptr ? std::array<std::int32_t, 3>{0, 0, 0} : coordinates->resolve(context.source());
}

std::shared_ptr<const CommandSource> wrapping(const CommandSource& base, ExecutionSource::Changes changes) {
    return std::make_shared<const ExecutionSource>(base, std::move(changes));
}

std::shared_ptr<const CommandSource> facing(const CommandSource& source, double x, double y, double z) {
    const std::array<double, 3> from = source.position();
    const ExecutionSource* execution = source.asExecution();
    const double eyeY = from[1] + (execution != nullptr && execution->anchorEyes() ? kEyeHeight : 0.0);
    const double dx = x - from[0];
    const double dy = y - eyeY;
    const double dz = z - from[2];
    return wrapping(source,
                    ExecutionSource::Changes{
                        .rotation = std::pair{static_cast<float>(-std::atan2(dx, dz) * kDegreesPerRadian),
                                              static_cast<float>(-std::atan2(dy, std::sqrt((dx * dx) + (dz * dz))) *
                                                                 kDegreesPerRadian)}});
}

void conditional(const CommandNodeBuilder& builder, CommandNode& execute, bool positive, Test test,
                 const bool terminal) {
    if (terminal) {
        builder.executes([test, positive](const CommandContext& context) -> Parsed<std::int32_t> {
            const std::int32_t count = counted(test, context, positive);
            context.success(count > 0 ? Component::translatable("commands.execute.conditional.pass_count",
                                                                {Component::text(std::to_string(count))})
                                      : Component::translatable("commands.execute.conditional.fail"));
            return count;
        });
    }
    builder.redirectTo(execute, true, [test, positive](const CommandContext& context) {
        std::vector<std::shared_ptr<const CommandSource>> onwards;
        if (counted(test, context, positive) > 0) {
            // Wrapped rather than handed on by reference: the chain outlives this call.
            onwards.push_back(wrapping(context.source(), {}));
        }
        return onwards;
    });
}

void storing(const CommandNodeBuilder& builder, CommandNode& execute, bool fromSuccess, WriteFactory write) {
    builder.redirectTo(execute, false, [write, fromSuccess](const CommandContext& context) {
        // Built here, while the context is alive: the callback below fires long after it is gone.
        Write bound = write(context);
        return std::vector<std::shared_ptr<const CommandSource>>{
            wrapping(context.source(),
                     ExecutionSource::Changes{.onResult = [bound, fromSuccess](bool success, std::int32_t result) {
                         bound(fromSuccess ? (success ? 1 : 0) : result);
                     }})};
    });
}

void typedStore(const CommandNodeBuilder& builder, CommandNode& execute, bool fromSuccess, TypedWriteFactory write) {
    for (const std::string_view type : kNumberTypes) {
        builder.literal(std::string{type}, [&execute, fromSuccess, write, type](const CommandNodeBuilder& form) {
            form.argument(
                "scale", doubleArgument(), [&execute, fromSuccess, write, type](const CommandNodeBuilder& scale) {
                    storing(scale, execute, fromSuccess, [write, type](const CommandContext& context) -> Write {
                        const auto* factor = context.get<double>("scale");
                        const double scaled = factor == nullptr ? 1.0 : *factor;
                        TypedWrite bound = write(context, type);
                        return [bound, scaled](std::int32_t value) { bound(static_cast<double>(value) * scaled); };
                    });
                });
        });
    }
}

} // namespace executeCommand

ExecuteCommand::ExecuteCommand(scoreboard::Scoreboard& scoreboard, Hooks hooks)
    : Command("execute", permissionLevel::kGamemasters), scoreboard_(&scoreboard), hooks_(hooks) {}

void ExecuteCommand::build(const CommandNodeBuilder& root) const {
    CommandNode& execute = root.node();

    root.literal("run", [&root](const CommandNodeBuilder& run) { run.redirectTo(root.root()); });
    buildContextClauses(root, execute);
    for (const bool positive : {true, false}) {
        root.literal(positive ? "if" : "unless", [this, &execute, positive](const CommandNodeBuilder& form) {
            buildConditions(form, execute, positive);
        });
    }
    root.literal("store", [this, &execute](const CommandNodeBuilder& store) {
        for (const bool fromSuccess : {false, true}) {
            store.literal(fromSuccess ? "success" : "result",
                          [this, &execute, fromSuccess](const CommandNodeBuilder& form) {
                              buildStores(form, execute, fromSuccess);
                          });
        }
    });
}

} // namespace basalt::server::runtime::command::commands
