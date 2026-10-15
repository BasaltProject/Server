#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/FunctionRunner.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::text::Component;

[[nodiscard]] CommandSyntaxError failure(Component component) {
    return CommandSyntaxError{.component = std::move(component)};
}

} // namespace

Parsed<std::vector<std::string>> FunctionRunner::instantiate(const CommandDispatcher& dispatcher,
                                                             const CommandSource& source,
                                                             const function::ServerFunction& function,
                                                             const NbtCompound* arguments) const {
    std::vector<std::string> lines;
    lines.reserve(function.entries.size());
    if (!function.isMacro) {
        for (const function::FunctionEntry& entry : function.entries) {
            lines.push_back(entry.command);
        }
        return lines;
    }
    const Component id = Component::text(function.id.toString());
    if (arguments == nullptr) {
        return std::unexpected{failure(Component::translatable("commands.function.error.missing_arguments", {id}))};
    }
    std::vector<std::string> values;
    for (const std::string& parameter : function.parameters) {
        const wire::nbt::NbtTag* value = arguments->find(parameter);
        if (value == nullptr) {
            return std::unexpected{failure(
                Component::translatable("commands.function.error.missing_argument", {id, Component::text(parameter)}))};
        }
        values.push_back(function::macroArgument(*value));
    }
    // parity: a macro line is parsed with the function's compilation source when it is filled in.
    const ExecutionSource compilation{
        source, ExecutionSource::Changes{.permission = settings_.permissionLevel, .silent = true, .trusted = true}};
    for (const function::FunctionEntry& entry : function.entries) {
        if (!entry.macro.has_value()) {
            lines.push_back(entry.command);
            continue;
        }
        std::string command = entry.macro->substitute(values);
        if (const std::optional<CommandSyntaxError> error = dispatcher.check(compilation, command); error.has_value()) {
            const std::string reason = wire::text::resolvePlainText(error->component, dispatcher.environment().lang());
            return std::unexpected{failure(Component::translatable(
                "commands.function.error.parse", {id, Component::text(command), Component::text(reason)}))};
        }
        lines.push_back(std::move(command));
    }
    return lines;
}

Parsed<std::optional<FunctionRunner::Returned>> FunctionRunner::call(const CommandDispatcher& dispatcher,
                                                                     const CommandSource& source,
                                                                     const function::ServerFunction& function,
                                                                     const NbtCompound* arguments) {
    Parsed<std::vector<std::string>> lines = instantiate(dispatcher, source, function, arguments);
    if (!lines.has_value()) {
        return std::unexpected{lines.error()};
    }
    if (frames_.empty()) {
        quota_ = 0;
        exhausted_ = false;
        limit_ = std::max(1, settings_.commandLimit ? settings_.commandLimit(source) : 65536);
    }
    const std::size_t base = frames_.size();
    frames_.push_back(Frame{.source = &source, .owned = nullptr, .lines = std::move(*lines), .onDone = {}});
    std::vector<Frame> outerDeferred = std::move(deferred_);
    deferred_.clear();
    const std::optional<Returned> outerReturn = pendingReturn_;
    pendingReturn_.reset();
    const std::optional<Returned> returned = runFrames(dispatcher, base);
    pendingReturn_ = outerReturn;
    deferred_ = std::move(outerDeferred);
    return returned;
}

std::shared_ptr<const CommandSource> FunctionRunner::keep(const CommandSource& source) const {
    const ExecutionSource* execution = source.asExecution();
    const CommandSource& outermost =
        frames_.front().owned != nullptr ? *frames_.front().owned : *frames_.front().source;
    return std::make_shared<const ExecutionSource>(
        outermost,
        ExecutionSource::Changes{.player = source.player(),
                                 .position = source.position(),
                                 .world = execution == nullptr ? std::nullopt : std::optional{execution->world()},
                                 .rotation = source.rotation(),
                                 .anchorEyes = execution != nullptr && execution->anchorEyes(),
                                 .entity = execution == nullptr ? std::nullopt : execution->entity(),
                                 .onResult = {},
                                 .permission = source.permissionLevel(),
                                 .silent = source.silent(),
                                 .trusted = true});
}

Parsed<void> FunctionRunner::defer(const CommandDispatcher& dispatcher, const CommandSource& source,
                                   const function::ServerFunction& function, const NbtCompound* arguments,
                                   Done onDone) {
    Parsed<std::vector<std::string>> lines = instantiate(dispatcher, source, function, arguments);
    if (!lines.has_value()) {
        return std::unexpected{lines.error()};
    }
    // The source this line built dies with the line, so what it says is copied onto the outermost one.
    deferred_.push_back(
        Frame{.source = nullptr, .owned = keep(source), .lines = std::move(*lines), .onDone = std::move(onDone)});
    return {};
}

void FunctionRunner::deferUntilReturn(const CommandDispatcher& dispatcher, std::shared_ptr<const CommandSource> source,
                                      std::vector<const function::ServerFunction*> functions, Done onDone) {
    if (functions.empty()) {
        onDone(std::nullopt);
        return;
    }
    const function::ServerFunction* first = functions.front();
    functions.erase(functions.begin());
    const CommandDispatcher* at = &dispatcher;
    auto rest = [this, at, source, functions = std::move(functions),
                 onDone](const std::optional<Returned>& returned) mutable {
        if (returned.has_value() || functions.empty()) {
            onDone(returned);
            return;
        }
        deferUntilReturn(*at, source, std::move(functions), std::move(onDone));
    };
    // parity: an instantiation failure here is `commands.execute.function.instantiationFailure`.
    if (!defer(dispatcher, *source, *first, nullptr, std::move(rest)).has_value()) {
        onDone(std::nullopt);
    }
}

void FunctionRunner::settle(const CommandDispatcher& dispatcher, Frame& finished) {
    if (!finished.onDone) {
        return;
    }
    const std::optional<Returned> returned = finished.returned;
    Done onDone = std::move(finished.onDone);
    pendingReturn_.reset();
    onDone(returned);
    static_cast<void>(dispatcher);
}

std::optional<FunctionRunner::Returned> FunctionRunner::runFrames(const CommandDispatcher& dispatcher,
                                                                  std::size_t base) {
    // parity: `ExecutionContext.runCommandQueue`.
    while (frames_.size() > base) {
        Frame& top = frames_.back();
        if (exhausted_ || top.returned.has_value() || top.next >= top.lines.size()) {
            Frame finished = std::move(top);
            frames_.pop_back();
            if (frames_.size() == base) {
                return finished.returned;
            }
            if (!exhausted_) {
                settle(dispatcher, finished);
                afterCommand();
            }
            continue;
        }
        if (quota_ >= limit_) {
            exhausted_ = true;
            dispatcher.environment().logger().info([this] {
                return "Command execution stopped due to limit (executed " + std::to_string(limit_) + " commands)";
            });
            continue;
        }
        ++quota_;
        const std::string line = top.lines[top.next++];
        const std::shared_ptr<const CommandSource> owned = top.owned;
        const CommandSource& from = owned != nullptr ? *owned : *top.source;
        // parity: the lines were compiled with the function's permission level, not the caller's.
        const ExecutionSource running{from, ExecutionSource::Changes{.trusted = true}};
        pendingReturn_.reset();
        static_cast<void>(dispatcher.execute(running, line));
        afterCommand();
    }
    return std::nullopt;
}

void FunctionRunner::afterCommand() {
    std::vector<Frame> queued = std::move(deferred_);
    deferred_.clear();
    if (pendingReturn_.has_value()) {
        // parity: a return discards what the returning frame queued.
        frames_.back().returned = pendingReturn_;
        pendingReturn_.reset();
        return;
    }
    for (auto frame = queued.rbegin(); frame != queued.rend(); ++frame) {
        frames_.push_back(std::move(*frame));
    }
}

Parsed<std::int32_t> FunctionRunner::run(const CommandDispatcher& dispatcher, const CommandSource& source,
                                         const Identifier& id, const NbtCompound* arguments) {
    const function::ServerFunction* function = library_->get(id);
    if (function == nullptr) {
        return std::unexpected{
            failure(Component::translatable("arguments.function.unknown", {Component::text(id.toString())}))};
    }
    const Parsed<std::optional<Returned>> ran = call(dispatcher, source, *function, arguments);
    if (!ran.has_value()) {
        return std::unexpected{ran.error()};
    }
    return ran->has_value() ? (*ran)->value : 0;
}

void FunctionRunner::runTag(const CommandDispatcher& dispatcher, const CommandSource& source, const Identifier& tag) {
    for (const function::ServerFunction* function : library_->tag(tag)) {
        // parity: an instantiation failure here is dropped without a word.
        static_cast<void>(call(dispatcher, source, *function));
    }
}

void FunctionRunner::tick(const CommandDispatcher& dispatcher, const CommandSource& gameLoop) {
    if (runLoad_) {
        runLoad_ = false;
        runTag(dispatcher, gameLoop, Identifier::vanilla("load"));
    }
    runTag(dispatcher, gameLoop, Identifier::vanilla("tick"));
}

void FunctionSchedule::schedule(const Identifier& id, bool isTag, std::int64_t triggerTime) {
    events_.push_back(Event{.name = (isTag ? "#" : "") + id.toString(),
                            .id = id,
                            .isTag = isTag,
                            .triggerTime = triggerTime,
                            .sequence = sequence_++});
}

std::int32_t FunctionSchedule::remove(std::string_view name) {
    const std::size_t before = events_.size();
    std::erase_if(events_, [name](const Event& event) { return event.name == name; });
    return static_cast<std::int32_t>(before - events_.size());
}

std::vector<std::string> FunctionSchedule::names() const {
    std::vector<std::string> names;
    for (const Event& event : events_) {
        if (!std::ranges::contains(names, event.name)) {
            names.push_back(event.name);
        }
    }
    return names;
}

std::vector<FunctionSchedule::Event> FunctionSchedule::due(std::int64_t gameTime) {
    std::vector<Event> ready;
    for (const Event& event : events_) {
        if (event.triggerTime <= gameTime) {
            ready.push_back(event);
        }
    }
    std::erase_if(events_, [gameTime](const Event& event) { return event.triggerTime <= gameTime; });
    std::ranges::sort(ready, [](const Event& left, const Event& right) {
        return left.triggerTime != right.triggerTime ? left.triggerTime < right.triggerTime
                                                     : left.sequence < right.sequence;
    });
    return ready;
}

wire::nbt::NbtCompound FunctionSchedule::toNbt() const {
    std::vector<Event> ordered = events_;
    std::ranges::sort(ordered, [](const Event& left, const Event& right) {
        return left.triggerTime != right.triggerTime ? left.triggerTime < right.triggerTime
                                                     : left.sequence < right.sequence;
    });
    wire::nbt::NbtList events;
    for (const Event& event : ordered) {
        NbtCompound callback;
        callback.putString("type", event.isTag ? "minecraft:function_tag" : "minecraft:function");
        callback.putString("id", event.id.toString());
        NbtCompound one;
        one.putLong("trigger_time", event.triggerTime);
        one.putString("id", event.name);
        one.set("callback", wire::nbt::NbtTag::ofCompound(std::move(callback)));
        static_cast<void>(events.push(wire::nbt::NbtTag::ofCompound(std::move(one))));
    }
    NbtCompound root;
    root.set("events", wire::nbt::NbtTag::ofList(std::move(events)));
    return root;
}

void FunctionSchedule::readNbt(const NbtCompound& saved) {
    events_.clear();
    const wire::nbt::NbtList* events = saved.getList("events");
    if (events == nullptr) {
        return;
    }
    for (const wire::nbt::NbtTag& tag : *events) {
        const auto* one = tag.get<NbtCompound>();
        const NbtCompound* callback = one == nullptr ? nullptr : one->getCompound("callback");
        if (callback == nullptr) {
            continue;
        }
        const std::string_view type = callback->getString("type");
        const std::expected<Identifier, wire::common::IdentifierError> id =
            Identifier::parse(callback->getString("id"));
        if (!id.has_value() || (type != "minecraft:function" && type != "minecraft:function_tag")) {
            continue;
        }
        events_.push_back(Event{.name = std::string{one->getString("id")},
                                .id = *id,
                                .isTag = type == "minecraft:function_tag",
                                .triggerTime = one->getLong("trigger_time"),
                                .sequence = sequence_++});
    }
}

} // namespace basalt::server::runtime::command::commands
