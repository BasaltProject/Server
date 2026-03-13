#include "ExecuteCommand.hpp"

#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>
#include <basalt/server/runtime/command/commands/FunctionCommands.hpp>
#include <basalt/wire/nbt/Snbt.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::text::Component;

[[nodiscard]] std::vector<Identifier> functionIds(const function::FunctionLibrary& library) {
    std::vector<Identifier> ids;
    for (const function::ServerFunction* one : library.all()) {
        ids.push_back(one->id);
    }
    return ids;
}

} // namespace

Parsed<std::string> FunctionRunner::expand(const std::string& line, const NbtCompound* arguments) const {
    if (!line.starts_with('$')) {
        return line;
    }
    const std::string body = line.substr(1);
    if (arguments == nullptr) {
        return std::unexpected{
            CommandSyntaxError{.component = Component::translatable("commands.function.error.missing_arguments")}};
    }
    std::string out;
    std::size_t index = 0;
    while (index < body.size()) {
        const std::size_t open = body.find("$(", index);
        if (open == std::string::npos) {
            out.append(body, index, body.size() - index);
            break;
        }
        const std::size_t close = body.find(')', open);
        if (close == std::string::npos) {
            out.append(body, index, body.size() - index);
            break;
        }
        out.append(body, index, open - index);
        const std::string name = body.substr(open + 2, close - open - 2);
        const wire::nbt::NbtTag* value = arguments->find(name);
        if (value == nullptr) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.function.error.parameter_not_found",
                                                                        {Component::text(name)})}};
        }
        const auto* text = value->get<std::string>();
        out += text != nullptr ? *text : wire::nbt::snbt::stringify(*value);
        index = close + 1;
    }
    return out;
}

Parsed<std::int32_t> FunctionRunner::run(const CommandDispatcher& dispatcher, const CommandSource& source,
                                         const Identifier& id, const NbtCompound* arguments) {
    const function::ServerFunction* function = library_->get(id);
    if (function == nullptr) {
        return std::unexpected{CommandSyntaxError{
            .component = Component::translatable("arguments.function.unknown", {Component::text(id.toString())})}};
    }
    if (depth_ >= kMaxDepth) {
        return std::unexpected{
            CommandSyntaxError{.component = Component::text("Function '" + id.toString() + "' recursed more than " +
                                                            std::to_string(kMaxDepth) + " deep and was stopped.")}};
    }
    depth_++;
    const std::optional<std::int32_t> outerReturn = pendingReturn_;
    pendingReturn_.reset();

    std::int32_t result = 0;
    std::optional<CommandSyntaxError> failure;
    for (const std::string& line : function->lines) {
        const Parsed<std::string> expanded = expand(line, arguments);
        if (!expanded.has_value()) {
            failure = expanded.error();
            break;
        }
        result = dispatcher.execute(source, *expanded);
        if (pendingReturn_.has_value()) {
            result = *pendingReturn_;
            break;
        }
    }
    pendingReturn_ = outerReturn;
    depth_--;
    if (failure.has_value()) {
        return std::unexpected{*failure};
    }
    return result;
}

std::int64_t FunctionSchedule::schedule(const Identifier& id, std::int32_t delayTicks, bool replace) {
    if (replace) {
        std::erase_if(entries_, [&id](const Entry& entry) { return entry.id == id; });
    }
    const std::int64_t runAt = tick_ + delayTicks;
    entries_.push_back(Entry{.id = id, .runAtTick = runAt});
    return runAt;
}

std::vector<Identifier> FunctionSchedule::scheduledIds() const {
    std::vector<Identifier> ids;
    for (const Entry& entry : entries_) {
        if (std::ranges::find(ids, entry.id) == ids.end()) {
            ids.push_back(entry.id);
        }
    }
    return ids;
}

std::int32_t FunctionSchedule::clear(const Identifier& id) {
    const std::size_t before = entries_.size();
    std::erase_if(entries_, [&id](const Entry& entry) { return entry.id == id; });
    return static_cast<std::int32_t>(before - entries_.size());
}

std::vector<Identifier> FunctionSchedule::tick() {
    tick_++;
    std::vector<Identifier> due;
    for (const Entry& entry : entries_) {
        if (entry.runAtTick <= tick_) {
            due.push_back(entry.id);
        }
    }
    std::erase_if(entries_, [this](const Entry& entry) { return entry.runAtTick <= tick_; });
    return due;
}

FunctionCommand::FunctionCommand(const function::FunctionLibrary& library, FunctionRunner& runner,
                                 const DataAccess* data)
    : Command("function", permissionLevel::kGamemasters), library_(&library), runner_(&runner), data_(data) {}

void FunctionCommand::build(const CommandNodeBuilder& root) const {
    /// Runs the function with whatever arguments a macro needs, and reports what it returned.
    const auto call = [this](const CommandContext& context, const NbtCompound* arguments) -> Parsed<std::int32_t> {
        const auto* id = context.get<Identifier>("name");
        if (id == nullptr) {
            return 0;
        }
        const Parsed<std::int32_t> result = runner_->run(context.dispatcher(), context.source(), *id, arguments);
        if (!result.has_value()) {
            return std::unexpected{result.error()};
        }
        context.success(Component::translatable("commands.function.result", {Component::text(id->toString()),
                                                                             Component::text(std::to_string(*result))}),
                        true);
        return *result;
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
        name.suggestsFromServer(
            [this](const SuggestionContext& context) { return context.matchingIds(functionIds(*library_)); });
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
                        return DataHolderRef{
                            DataHolderRef::Block{.pos = coordinates->resolve(context.source().position())}};
                    });
                });
            });
            with.literal("entity", [pathed](const CommandNodeBuilder& form) {
                form.argument("source", entitiesArgument(true), [pathed](const CommandNodeBuilder& source) {
                    pathed(source, [](const CommandContext& context) -> std::optional<DataHolderRef> {
                        const auto* matched = context.get<std::vector<CommandTarget>>("source");
                        player::PlayerSession* session =
                            matched == nullptr || matched->size() != 1 ? nullptr : matched->front().player();
                        if (session == nullptr) {
                            return std::nullopt;
                        }
                        return DataHolderRef{DataHolderRef::Entity{.player = session}};
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
    const auto returnWith = [this](std::int32_t value) -> Parsed<std::int32_t> {
        if (!runner_->insideFunction()) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::text("/return only works inside a function.")}};
        }
        runner_->requestReturn(value);
        return value;
    };
    root.argument("value", integerArgument(), [returnWith](const CommandNodeBuilder& value) {
        value.executes([returnWith](const CommandContext& context) {
            const auto* asked = context.get<std::int32_t>("value");
            return returnWith(asked == nullptr ? 0 : *asked);
        });
    });
    root.literal("fail", [returnWith](const CommandNodeBuilder& fail) {
        fail.executes([returnWith](const CommandContext&) { return returnWith(0); });
    });
    // parity: `run` forwards to the root, so the client completes the nested command, and its
    // result becomes the function's.
    root.literal("run", [this, &root](const CommandNodeBuilder& run) {
        run.redirectTo(root.root(), false, [runner = runner_](const CommandContext& context) {
            return std::vector<std::shared_ptr<const CommandSource>>{executeCommand::wrapping(
                context.source(), ExecutionSource::Changes{.onResult = [runner](bool success, std::int32_t result) {
                    if (runner->insideFunction()) {
                        runner->requestReturn(success ? result : 0);
                    }
                }})};
        });
    });
}

ScheduleCommand::ScheduleCommand(const function::FunctionLibrary& library, FunctionSchedule& schedule)
    : Command("schedule", permissionLevel::kGamemasters), library_(&library), schedule_(&schedule) {}

void ScheduleCommand::build(const CommandNodeBuilder& root) const {
    const auto later = [this](const CommandContext& context, bool replace) -> Parsed<std::int32_t> {
        const auto* id = context.get<Identifier>("function");
        const auto* delay = context.get<std::int32_t>("time");
        if (id == nullptr || delay == nullptr) {
            return 0;
        }
        const std::int64_t runAt = schedule_->schedule(*id, *delay, replace);
        context.success(
            Component::translatable("commands.schedule.created.function",
                                    {Component::text(id->toString()), Component::text(std::to_string(*delay)),
                                     Component::text(std::to_string(runAt))}),
            true);
        return *delay;
    };
    root.literal("function", [this, later](const CommandNodeBuilder& form) {
        form.argument("function", functionArgument(), [this, later](const CommandNodeBuilder& named) {
            named.suggestsFromServer(
                [this](const SuggestionContext& context) { return context.matchingIds(functionIds(*library_)); });
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
        // parity: a greedy string, not a function name: a scheduled entry can be a `#tag` too.
        form.argument("function", greedyStringArgument(), [this](const CommandNodeBuilder& named) {
            named.suggestsFromServer(
                [this](const SuggestionContext& context) { return context.matchingIds(schedule_->scheduledIds()); });
            named.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto* text = context.get<std::string>("function");
                const std::expected<Identifier, wire::common::IdentifierError> id =
                    text == nullptr ? std::unexpected{wire::common::IdentifierError::EmptyPart}
                                    : Identifier::parse(*text);
                const std::int32_t cleared = id.has_value() ? schedule_->clear(*id) : 0;
                const Component named_ =
                    Component::text(id.has_value() ? id->toString() : (text == nullptr ? std::string{} : *text));
                if (cleared == 0) {
                    return std::unexpected{CommandSyntaxError{
                        .component = Component::translatable("commands.schedule.cleared.failure", {named_})}};
                }
                context.success(Component::translatable("commands.schedule.cleared.success",
                                                        {Component::text(std::to_string(cleared)), named_}),
                                true);
                return cleared;
            });
        });
    });
}

DataPackCommand::DataPackCommand(function::FunctionLibrary& library)
    : Command("datapack", permissionLevel::kGamemasters), library_(&library) {}

void DataPackCommand::build(const CommandNodeBuilder& root) const {
    const auto list = [this](const CommandContext& context, std::optional<bool> enabledOnly) -> Parsed<std::int32_t> {
        std::vector<std::string> packs;
        if (!enabledOnly.has_value()) {
            packs = library_->availablePacks();
        } else if (*enabledOnly) {
            packs = library_->enabledPacks();
        } else {
            for (const std::string& pack : library_->availablePacks()) {
                if (!library_->isEnabled(pack)) {
                    packs.push_back(pack);
                }
            }
        }
        const std::string key = enabledOnly.value_or(false) ? "enabled" : "available";
        std::string named;
        for (const std::string& pack : packs) {
            named += named.empty() ? pack : ", " + pack;
        }
        context.success(packs.empty() ? Component::translatable("commands.datapack.list." + key + ".none")
                                      : Component::translatable(
                                            "commands.datapack.list." + key + ".success",
                                            {Component::text(std::to_string(packs.size())), Component::text(named)}));
        return static_cast<std::int32_t>(packs.size());
    };

    const auto setEnabled = [this](const CommandContext& context, bool enabled,
                                   const function::FunctionLibrary::Position& position) -> Parsed<std::int32_t> {
        const auto* name = context.get<std::string>("name");
        if (name == nullptr) {
            return 0;
        }
        if (!library_->setEnabled(*name, enabled, position)) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable(enabled ? "commands.datapack.enable.failed"
                                                                                : "commands.datapack.disable.failed",
                                                                        {Component::text(*name)})}};
        }
        context.success(
            Component::translatable(enabled ? "commands.datapack.modify.enable" : "commands.datapack.modify.disable",
                                    {Component::text(*name)}),
            true);
        return static_cast<std::int32_t>(library_->enabledPacks().size());
    };

    root.literal("list", [list](const CommandNodeBuilder& form) {
        form.executes([list](const CommandContext& context) { return list(context, std::nullopt); });
        form.literal("available", [list](const CommandNodeBuilder& one) {
            one.executes([list](const CommandContext& context) { return list(context, false); });
        });
        form.literal("enabled", [list](const CommandNodeBuilder& one) {
            one.executes([list](const CommandContext& context) { return list(context, true); });
        });
    });

    root.literal("enable", [this, setEnabled](const CommandNodeBuilder& form) {
        form.argument("name", wordArgument(), [this, setEnabled](const CommandNodeBuilder& name) {
            name.suggestsFromServer([this](const SuggestionContext& context) {
                std::vector<std::string> candidates;
                for (const std::string& pack : library_->availablePacks()) {
                    if (!library_->isEnabled(pack)) {
                        candidates.push_back(pack);
                    }
                }
                return context.matching(candidates);
            });
            name.executes([setEnabled](const CommandContext& context) {
                return setEnabled(context, true, function::FunctionLibrary::Position::last());
            });
            name.literal("first", [setEnabled](const CommandNodeBuilder& one) {
                one.executes([setEnabled](const CommandContext& context) {
                    return setEnabled(context, true, function::FunctionLibrary::Position::first());
                });
            });
            name.literal("last", [setEnabled](const CommandNodeBuilder& one) {
                one.executes([setEnabled](const CommandContext& context) {
                    return setEnabled(context, true, function::FunctionLibrary::Position::last());
                });
            });
            for (const bool before : {true, false}) {
                name.literal(before ? "before" : "after", [this, setEnabled, before](const CommandNodeBuilder& one) {
                    one.argument("existing", wordArgument(),
                                 [this, setEnabled, before](const CommandNodeBuilder& existing) {
                                     existing.suggestsFromServer([this](const SuggestionContext& context) {
                                         return context.matching(library_->enabledPacks());
                                     });
                                     existing.executes([setEnabled, before](const CommandContext& context) {
                                         const auto* other = context.get<std::string>("existing");
                                         const std::string named = other == nullptr ? std::string{} : *other;
                                         return setEnabled(context, true,
                                                           before ? function::FunctionLibrary::Position::before(named)
                                                                  : function::FunctionLibrary::Position::after(named));
                                     });
                                 });
                });
            }
        });
    });

    root.literal("disable", [this, setEnabled](const CommandNodeBuilder& form) {
        form.argument("name", wordArgument(), [this, setEnabled](const CommandNodeBuilder& name) {
            name.suggestsFromServer(
                [this](const SuggestionContext& context) { return context.matching(library_->enabledPacks()); });
            name.executes([setEnabled](const CommandContext& context) {
                return setEnabled(context, false, function::FunctionLibrary::Position::last());
            });
        });
    });

    /// parity: `/datapack create`, which writes an empty pack and its `pack.mcmeta`.
    root.literal("create", [this](const CommandNodeBuilder& form) {
        form.argument("id", wordArgument(), [this](const CommandNodeBuilder& id) {
            id.argument("description", componentArgument(), [this](const CommandNodeBuilder& description) {
                description.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                    const auto* name = context.get<std::string>("id");
                    const auto* text = context.get<Component>("description");
                    if (name == nullptr) {
                        return 0;
                    }
                    if (!library_->create(*name, text == nullptr ? std::string{} : wire::text::toPlainText(*text))) {
                        return std::unexpected{CommandSyntaxError{
                            .component = Component::translatable("commands.datapack.create.already_exists",
                                                                 {Component::text(*name)})}};
                    }
                    context.success(
                        Component::translatable("commands.datapack.create.success", {Component::text(*name)}), true);
                    return 1;
                });
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
