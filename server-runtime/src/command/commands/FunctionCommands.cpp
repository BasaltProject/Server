#include "ExecuteCommand.hpp"

#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>
#include <basalt/server/runtime/command/commands/FunctionCommands.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::text::Component;

/// parity: `FunctionArgument.getFunctions`: a tag's members, or the one function.
[[nodiscard]] Parsed<std::vector<const function::ServerFunction*>> functionsOf(const function::FunctionLibrary& library,
                                                                               const FunctionName& name) {
    if (name.isTag) {
        if (!library.hasTag(name.id)) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("arguments.function.tag.unknown",
                                                                        {Component::text(name.id.toString())})}};
        }
        return library.tag(name.id);
    }
    const function::ServerFunction* one = library.get(name.id);
    if (one == nullptr) {
        return std::unexpected{CommandSyntaxError{
            .component = Component::translatable("arguments.function.unknown", {Component::text(name.id.toString())})}};
    }
    return std::vector<const function::ServerFunction*>{one};
}

[[nodiscard]] std::string displayed(const FunctionName& name) { return (name.isTag ? "#" : "") + name.id.toString(); }

} // namespace

std::vector<std::string> functionSuggestions(const function::FunctionLibrary& library) {
    // parity: `SUGGEST_FUNCTION`, the tags with their `#` first, then the functions.
    std::vector<std::string> names;
    for (const Identifier& tag : library.tagIds()) {
        names.push_back("#" + tag.toString());
    }
    for (const Identifier& id : library.functionIds()) {
        names.push_back(id.toString());
    }
    return names;
}

FunctionCommand::FunctionCommand(FunctionRunner& runner, const DataAccess* data)
    : Command("function", permissionLevel::kGamemasters), runner_(&runner), data_(data) {}

void FunctionCommand::build(const CommandNodeBuilder& root) const {
    const auto call = [this](const CommandContext& context, const NbtCompound* arguments) -> Parsed<std::int32_t> {
        const auto* name = context.get<FunctionName>("name");
        if (name == nullptr) {
            return 0;
        }
        const Parsed<std::vector<const function::ServerFunction*>> functions = functionsOf(runner_->library(), *name);
        if (!functions.has_value()) {
            return std::unexpected{functions.error()};
        }
        if (functions->empty()) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.function.scheduled.no_functions",
                                                                        {Component::text(displayed(*name))})}};
        }
        if (functions->size() == 1) {
            context.success(Component::translatable("commands.function.scheduled.single",
                                                    {Component::text(functions->front()->id.toString())}),
                            true);
        } else {
            std::vector<Component> ids;
            for (const function::ServerFunction* one : *functions) {
                ids.push_back(Component::text(one->id.toString()));
            }
            context.success(Component::translatable("commands.function.scheduled.multiple",
                                                    {Component::join(Component::text(", "), ids)}),
                            true);
        }
        // parity: `modifySenderForExecution`, silent and at least a game master.
        const ExecutionSource running{
            context.source(), ExecutionSource::Changes{.permission = std::max(context.source().permissionLevel(),
                                                                              permissionLevel::kGamemasters),
                                                       .silent = true}};
        if (runner_->insideFunction()) {
            // parity: `queueFunctions`.
            const ExecutionSource* execution = context.source().asExecution();
            std::vector<ExecutionSource::OnResult> waiting =
                execution == nullptr ? std::vector<ExecutionSource::OnResult>{} : execution->resultCallbacks();
            struct Sum {
                std::size_t left = 0;
                bool any = false;
                std::int32_t total = 0;
            };
            auto sum = std::make_shared<Sum>(Sum{.left = functions->size()});
            for (const function::ServerFunction* one : *functions) {
                const Parsed<void> queued =
                    runner_->defer(context.dispatcher(), running, *one, arguments,
                                   [sum, waiting](const std::optional<FunctionRunner::Returned>& returned) {
                                       if (returned.has_value()) {
                                           sum->any = true;
                                           sum->total += returned->value;
                                       }
                                       if (--sum->left == 0 && sum->any) {
                                           for (const ExecutionSource::OnResult& told : waiting) {
                                               told(true, sum->total);
                                           }
                                       }
                                   });
                if (!queued.has_value()) {
                    return std::unexpected{CommandSyntaxError{
                        .component =
                            Component::translatable("commands.function.instantiationFailure",
                                                    {Component::text(one->id.toString()), queued.error().component})}};
                }
            }
            if (!waiting.empty()) {
                CommandDispatcher::deferResult();
            }
            return 0;
        }
        std::int32_t total = 0;
        for (const function::ServerFunction* one : *functions) {
            const Parsed<std::optional<FunctionRunner::Returned>> ran =
                runner_->call(context.dispatcher(), running, *one, arguments);
            if (!ran.has_value()) {
                return std::unexpected{CommandSyntaxError{
                    .component =
                        Component::translatable("commands.function.instantiationFailure",
                                                {Component::text(one->id.toString()), ran.error().component})}};
            }
            if (!ran->has_value()) {
                continue;
            }
            total += (*ran)->value;
            context.success(
                Component::translatable("commands.function.result", {Component::text(one->id.toString()),
                                                                     Component::text(std::to_string((*ran)->value))}),
                true);
        }
        return total;
    };

    const auto macroFrom = [this, call](const CommandContext& context, const DataHolderRef& holder,
                                        bool pathed) -> Parsed<std::int32_t> {
        const auto* path = pathed ? context.get<std::string>("path") : nullptr;
        const std::optional<NbtCompound> arguments =
            data_ == nullptr ? std::nullopt : data_->readCompound(context, holder, path);
        if (!arguments.has_value()) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.function.error.missing_arguments")}};
        }
        return call(context, &*arguments);
    };

    root.argument("name", functionArgument(), [this, call, macroFrom](const CommandNodeBuilder& name) {
        name.suggestsFromServer([this](const SuggestionContext& context) {
            return context.matching(functionSuggestions(runner_->library()));
        });
        name.executes([call](const CommandContext& context) { return call(context, nullptr); });
        name.argument("arguments", nbtCompoundArgument(), [call](const CommandNodeBuilder& arguments) {
            arguments.executes(
                [call](const CommandContext& context) { return call(context, context.get<NbtCompound>("arguments")); });
        });
        name.literal("with", [macroFrom](const CommandNodeBuilder& with) {
            const auto pathed =
                [macroFrom](const CommandNodeBuilder& node,
                            const std::function<std::optional<DataHolderRef>(const CommandContext&)>& holderOf) {
                    const auto run = [macroFrom, holderOf](const CommandContext& context,
                                                           bool withPath) -> Parsed<std::int32_t> {
                        const std::optional<DataHolderRef> holder = holderOf(context);
                        if (!holder.has_value()) {
                            return std::unexpected{CommandSyntaxError{
                                .component = Component::translatable("commands.function.error.missing_arguments")}};
                        }
                        return macroFrom(context, *holder, withPath);
                    };
                    node.executes([run](const CommandContext& context) { return run(context, false); });
                    node.argument("path", nbtPathArgument(), [run](const CommandNodeBuilder& path) {
                        path.executes([run](const CommandContext& context) { return run(context, true); });
                    });
                };

            with.literal("block", [pathed](const CommandNodeBuilder& form) {
                form.argument("sourcePos", blockPosArgument(), [pathed](const CommandNodeBuilder& pos) {
                    pathed(pos, [](const CommandContext& context) -> std::optional<DataHolderRef> {
                        const auto* coordinates = context.get<BlockCoordinates>("sourcePos");
                        if (coordinates == nullptr) {
                            return std::nullopt;
                        }
                        return DataHolderRef{DataHolderRef::Block{.pos = coordinates->resolve(context.source())}};
                    });
                });
            });
            with.literal("entity", [pathed](const CommandNodeBuilder& form) {
                form.argument("source", entitiesArgument(true), [pathed](const CommandNodeBuilder& source) {
                    pathed(source, [](const CommandContext& context) -> std::optional<DataHolderRef> {
                        const auto* matched = context.get<std::vector<CommandTarget>>("source");
                        return matched == nullptr || matched->size() != 1 ? std::nullopt
                                                                          : entityHolderOf(matched->front());
                    });
                });
            });
            with.literal("storage", [pathed](const CommandNodeBuilder& form) {
                form.argument("source", resourceLocationArgument(), [pathed](const CommandNodeBuilder& source) {
                    pathed(source, [](const CommandContext& context) -> std::optional<DataHolderRef> {
                        const auto* id = context.get<Identifier>("source");
                        if (id == nullptr) {
                            return std::nullopt;
                        }
                        return DataHolderRef{DataHolderRef::Storage{.id = *id}};
                    });
                });
            });
        });
    });
}

ReturnCommand::ReturnCommand(FunctionRunner& runner)
    : Command("return", permissionLevel::kGamemasters), runner_(&runner) {}

void ReturnCommand::build(const CommandNodeBuilder& root) const {
    const auto returnWith = [this](bool success, std::int32_t value) -> Parsed<std::int32_t> {
        if (runner_->insideFunction()) {
            runner_->requestReturn(FunctionRunner::Returned{.success = success, .value = value});
        }
        return value;
    };
    root.argument("value", integerArgument(), [returnWith](const CommandNodeBuilder& value) {
        value.executes([returnWith](const CommandContext& context) {
            const auto* asked = context.get<std::int32_t>("value");
            return returnWith(true, asked == nullptr ? 0 : *asked);
        });
    });
    root.literal("fail", [returnWith](const CommandNodeBuilder& fail) {
        fail.executes([returnWith](const CommandContext&) { return returnWith(false, 0); });
    });
    // parity: `run` forwards to the root.
    root.literal("run", [this, &root](const CommandNodeBuilder& run) {
        run.redirectTo(root.root(), false, [runner = runner_](const CommandContext& context) {
            return std::vector<std::shared_ptr<const CommandSource>>{executeCommand::wrapping(
                context.source(), ExecutionSource::Changes{.onResult = [runner](bool success, std::int32_t result) {
                    if (runner->insideFunction()) {
                        runner->requestReturn(
                            FunctionRunner::Returned{.success = success, .value = success ? result : 0});
                    }
                }})};
        });
    });
}

ScheduleCommand::ScheduleCommand(const function::FunctionLibrary& library, FunctionSchedule& schedule,
                                 GameTime gameTime)
    : Command("schedule", permissionLevel::kGamemasters), library_(&library), schedule_(&schedule),
      gameTime_(std::move(gameTime)) {}

void ScheduleCommand::build(const CommandNodeBuilder& root) const {
    const auto later = [this](const CommandContext& context, bool replace) -> Parsed<std::int32_t> {
        const auto* name = context.get<FunctionName>("function");
        const auto* delay = context.get<std::int32_t>("time");
        if (name == nullptr || delay == nullptr) {
            return 0;
        }
        if (*delay == 0) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.schedule.same_tick")}};
        }
        const Parsed<std::vector<const function::ServerFunction*>> functions = functionsOf(*library_, *name);
        if (!functions.has_value()) {
            return std::unexpected{functions.error()};
        }
        if (!name->isTag && functions->front()->isMacro) {
            return std::unexpected{CommandSyntaxError{.component = Component::translatable("commands.schedule.macro")}};
        }
        const std::int64_t triggerTime = (gameTime_ ? gameTime_(context) : 0) + *delay;
        if (replace) {
            static_cast<void>(schedule_->remove(displayed(*name)));
        }
        schedule_->schedule(name->id, name->isTag, triggerTime);
        context.success(Component::translatable(
                            name->isTag ? "commands.schedule.created.tag" : "commands.schedule.created.function",
                            {Component::text(name->id.toString()), Component::text(std::to_string(*delay)),
                             Component::text(std::to_string(triggerTime))}),
                        true);
        // parity: `Math.floorMod(tickTime, Integer.MAX_VALUE)`.
        constexpr std::int64_t kIntMax = std::numeric_limits<std::int32_t>::max();
        return static_cast<std::int32_t>(((triggerTime % kIntMax) + kIntMax) % kIntMax);
    };
    root.literal("function", [this, later](const CommandNodeBuilder& form) {
        form.argument("function", functionArgument(), [this, later](const CommandNodeBuilder& named) {
            named.suggestsFromServer(
                [this](const SuggestionContext& context) { return context.matching(functionSuggestions(*library_)); });
            named.argument("time", timeArgument(0), [later](const CommandNodeBuilder& time) {
                time.executes([later](const CommandContext& context) { return later(context, true); });
                time.literal("append", [later](const CommandNodeBuilder& append) {
                    append.executes([later](const CommandContext& context) { return later(context, false); });
                });
                time.literal("replace", [later](const CommandNodeBuilder& replace) {
                    replace.executes([later](const CommandContext& context) { return later(context, true); });
                });
            });
        });
    });
    root.literal("clear", [this](const CommandNodeBuilder& form) {
        // parity: an `IdentifierArgument`, so a tag's `#` entry cannot be named here at all.
        form.argument("function", resourceLocationArgument(), [this](const CommandNodeBuilder& named) {
            named.suggestsFromServer(
                [this](const SuggestionContext& context) { return context.matching(schedule_->names()); });
            named.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto* id = context.get<Identifier>("function");
                const std::string name = id == nullptr ? std::string{} : id->toString();
                const std::int32_t cleared = schedule_->remove(name);
                if (cleared == 0) {
                    return std::unexpected{
                        CommandSyntaxError{.component = Component::translatable("commands.schedule.cleared.failure",
                                                                                {Component::text(name)})}};
                }
                context.success(
                    Component::translatable("commands.schedule.cleared.success",
                                            {Component::text(std::to_string(cleared)), Component::text(name)}),
                    true);
                return cleared;
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
