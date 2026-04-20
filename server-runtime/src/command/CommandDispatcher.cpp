#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace basalt::server::runtime::command {
namespace {

class DeferredSource final : public CommandSource {
public:
    explicit DeferredSource(const CommandSource& typist) : typist_(&typist) {}

    [[nodiscard]] std::string name() const override { return typist_->name(); }

    [[nodiscard]] wire::text::Component displayName() const override { return typist_->displayName(); }

    [[nodiscard]] std::int32_t permissionLevel() const override { return typist_->permissionLevel(); }

    [[nodiscard]] std::array<double, 3> position() const override { return typist_->position(); }

    [[nodiscard]] player::PlayerSession* player() const override { return typist_->player(); }

    void sendSystemMessage(const wire::text::Component& message) const override { typist_->sendSystemMessage(message); }

    [[nodiscard]] bool hasPermission(std::optional<std::string_view> node) const override {
        return typist_->hasPermission(node);
    }

    [[nodiscard]] const ExecutionSource* asExecution() const override { return typist_->asExecution(); }

    [[nodiscard]] bool defersSelectors() const override { return true; }

    [[nodiscard]] bool silent() const override { return typist_->silent(); }

private:
    const CommandSource* typist_;
};

/// Set by a command that answers later; the thread's own, since commands run on several.
thread_local bool resultDeferred = false;

using wire::protocol::play::CommandNodeData;
using wire::protocol::play::CommandsPacket;
using wire::protocol::play::CommandSuggestion;
using wire::text::Component;

[[nodiscard]] std::string lowered(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char letter : text) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(letter))));
    }
    return result;
}

[[nodiscard]] std::string firstWord(const std::string& input) {
    const std::size_t space = input.find(' ');
    return lowered(space == std::string::npos ? input : input.substr(0, space));
}

} // namespace

CommandFeedback::CommandFeedback(const CommandEnvironment& environment)
    : environment_(&environment),
      overrides_({environment.commands().messages().begin(), environment.commands().messages().end()},
                 environment.placeholders(), environment.lang()) {}

void CommandFeedback::success(const CommandSource& source, const Component& rawMessage, bool broadcastToAdmins) const {
    if (source.silent()) {
        return;
    }
    const Component message = overrides_.apply(rawMessage, source.player());
    source.sendSystemMessage(message);
    if (!broadcastToAdmins) {
        return;
    }
    const Component broadcast = Component::translatable("chat.type.admin", {source.displayName(), message})
                                    .color(wire::text::kGray)
                                    .italic(true);
    for (player::PlayerSession* player : environment_->players().snapshot()) {
        if (source.player() != nullptr && player->uuid() == source.player()->uuid()) {
            continue;
        }
        if (!environment_->groups().resolve(player->group, kAdminPermission)) {
            continue;
        }
        player->sendSystemMessage(broadcast);
    }
    if (source.player() != nullptr) {
        environment_->logger().info([&] { return wire::text::resolvePlainText(broadcast, environment_->lang()); });
    }
}

void CommandFeedback::failure(const CommandSource& source, const Component& message) const {
    source.sendSystemMessage(overrides_.apply(message, source.player()).color(wire::text::kRed));
}

CommandContext::CommandContext(const CommandSource& source, const CommandDispatcher& dispatcher, std::string input,
                               Arguments arguments)
    : source_(&source), dispatcher_(&dispatcher), input_(std::move(input)), arguments_(std::move(arguments)) {}

const CommandEnvironment& CommandContext::environment() const { return dispatcher_->environment(); }

bool CommandContext::has(std::string_view name) const { return arguments_.contains(name); }

void CommandContext::success(const Component& message, bool broadcastToAdmins) const {
    dispatcher_->feedback().success(*source_, message, broadcastToAdmins);
}

Parsed<player::PlayerSession*> CommandContext::requirePlayer() const {
    player::PlayerSession* player = source_->player();
    if (player == nullptr) {
        return std::unexpected{CommandSyntaxError{.component = Component::translatable("permissions.requires.player")}};
    }
    return player;
}

Parsed<world::LoadedWorld*> CommandContext::world() const {
    // parity: `CommandSourceStack.getLevel`.
    world::LoadedWorld* found = worldIn(*source_, environment());
    if (found == nullptr) {
        return std::unexpected{CommandSyntaxError{.component = Component::text("No world is loaded.")}};
    }
    return found;
}

Parsed<player::PlayerSession*> CommandContext::requireEntity() const {
    player::PlayerSession* player = source_->player();
    if (player == nullptr) {
        return std::unexpected{CommandSyntaxError{.component = Component::translatable("permissions.requires.entity")}};
    }
    return player;
}

namespace {

[[nodiscard]] std::string usageText(const CommandNode& node) {
    return node.kind() == NodeKind::Argument ? "<" + node.name() + ">" : node.name();
}

} // namespace

std::string smartUsage(const CommandNode& node, const CommandSource& source, bool optional, bool deep) {
    const std::string self = optional ? "[" + usageText(node) + "]" : usageText(node);
    if (deep) {
        return self;
    }
    const bool childOptional = static_cast<bool>(node.executor);
    const std::string open = childOptional ? "[" : "(";
    const std::string close = childOptional ? "]" : ")";
    const CommandNode& target = node.redirect == nullptr ? node : *node.redirect;
    std::vector<CommandNode*> children;
    for (CommandNode* child : target.sortedChildren()) {
        if (mayUse(source, *child)) {
            children.push_back(child);
        }
    }
    if (children.size() == 1) {
        return self + " " + smartUsage(*children.front(), source, childOptional, childOptional);
    }
    if (children.size() > 1) {
        std::vector<std::string> usages;
        for (const CommandNode* child : children) {
            std::string usage = smartUsage(*child, source, childOptional, true);
            if (std::ranges::find(usages, usage) == usages.end()) {
                usages.push_back(std::move(usage));
            }
        }
        if (usages.size() == 1) {
            return self + " " + (childOptional ? "[" + usages.front() + "]" : usages.front());
        }
        if (usages.size() > 1) {
            std::string joined;
            for (const std::string& usage : usages) {
                if (!joined.empty()) {
                    joined += "|";
                }
                joined += usage;
            }
            return self + " " + open + joined + close;
        }
    }
    return self;
}

std::vector<std::string> smartUsages(const CommandNode& node, const CommandSource& source) {
    // parity: `getSmartUsage` reads the node's own command for the children's optionality and skips a child the.
    const bool optional = static_cast<bool>(node.executor);
    std::vector<std::string> usages;
    for (const CommandNode* child : node.sortedChildren()) {
        if (child->kind() == NodeKind::Root || !mayUse(source, *child)) {
            continue;
        }
        usages.push_back(smartUsage(*child, source, optional, false));
    }
    return usages;
}

Command::Command(std::string name, std::int32_t level, std::vector<std::string> aliases,
                 std::optional<std::string> permission)
    : name_(std::move(name)), level_(level), aliases_(std::move(aliases)),
      permission_(permission.has_value() ? std::move(*permission) : "basalt.command." + name_) {}

CommandDispatcher::CommandDispatcher(const CommandEnvironment& environment)
    : environment_(&environment), root_(NodeKind::Root, ""), feedback_(environment) {
    registerCustom();
}

void CommandDispatcher::registerCustom() {}

CommandNode* CommandDispatcher::registerCommand(std::shared_ptr<const Command> command) {
    const auto same = std::ranges::find_if(registered_, [&command](const std::shared_ptr<const Command>& held) {
        return held->name() == command->name();
    });
    const Command& definition = *command;
    std::shared_ptr<const Command> replaced;
    if (same == registered_.end()) {
        registered_.push_back(std::move(command));
    } else {
        replaced = std::exchange(*same, std::move(command));
    }
    // The one it took the place of outlives its own nodes, which `registerBuilt` takes out first.
    return registerBuilt(definition);
}

CommandNode* CommandDispatcher::registerBuilt(const Command& command) {
    const config::CommandOverride& override = environment_->commands().get(command.name());
    if (!override.enabled) {
        return nullptr;
    }
    // A second registration replaces the first so clients do not see two roots with one name.
    const CommandNode* replaced = nullptr;
    for (const std::unique_ptr<CommandNode>& child : root_.children()) {
        if (child->kind() == NodeKind::Literal && child->redirect == nullptr && child->name() == command.name()) {
            replaced = child.get();
            break;
        }
    }
    if (replaced != nullptr) {
        unregisterNode(replaced);
    }

    auto node = std::make_unique<CommandNode>(NodeKind::Literal, command.name());
    node->requiredLevel = command.permissionLevel();
    node->permission = override.permission.has_value() ? *override.permission : command.permission();
    CommandNode& added = root_.addChild(std::move(node));
    command.build(CommandNodeBuilder{added, root_});
    for (const std::string& alias : command.aliases()) {
        static_cast<void>(registerAlias(alias, added));
    }
    for (const std::string& alias : override.aliases) {
        static_cast<void>(registerAlias(alias, added));
    }
    return &added;
}

CommandNode& CommandDispatcher::registerNode(std::string name,
                                             const std::function<void(const CommandNodeBuilder&)>& build) {
    CommandNode& added = root_.addChild(std::make_unique<CommandNode>(NodeKind::Literal, std::move(name)));
    build(CommandNodeBuilder{added, root_});
    return added;
}

void CommandDispatcher::unregisterNode(const CommandNode* node) {
    ++generation_;
    std::vector<const CommandNode*> aliases;
    for (const std::unique_ptr<CommandNode>& child : root_.children()) {
        if (child->redirect == node) {
            aliases.push_back(child.get());
        }
    }
    for (const CommandNode* alias : aliases) {
        root_.removeChild(alias);
    }
    root_.removeChild(node);
}

CommandNode& CommandDispatcher::registerAlias(std::string name, CommandNode& target) {
    auto node = std::make_unique<CommandNode>(NodeKind::Literal, std::move(name));
    node->requiredLevel = target.requiredLevel;
    node->permission = target.permission;
    node->redirect = &target;
    node->executor = target.executor;
    return root_.addChild(std::move(node));
}

void CommandDispatcher::rebuild() {
    ++generation_;
    root_.clearChildren();
    registerCustom();
    const std::vector<std::shared_ptr<const Command>> remembered = registered_;
    for (const std::shared_ptr<const Command>& command : remembered) {
        static_cast<void>(registerBuilt(*command));
    }
}

std::int32_t CommandDispatcher::execute(const CommandSource& source, const std::string& input) const {
    std::int32_t successes = 0;
    return execute(source, input, successes);
}

std::int32_t CommandDispatcher::execute(const CommandSource& source, const std::string& input,
                                        std::int32_t& successes) const {
    StringReader reader{input};
    const auto started = std::chrono::steady_clock::now();
    const Parsed<std::vector<Link>> links = parse(reader, root_, source, {});
    std::int32_t result = 0;
    if (!links.has_value()) {
        reportSyntaxError(source, links.error(), input);
    } else {
        const Parsed<std::int32_t> ran = runChain(*links, 0, source, input, reader, successes);
        if (ran.has_value()) {
            result = *ran;
        } else {
            reportSyntaxError(source, ran.error(), input);
        }
    }
    if (environment_->options().timings != nullptr) {
        environment_->options()
            .timings->of("command." + firstWord(input))
            .record(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started)
                        .count());
    }
    return result;
}

std::optional<CommandSyntaxError> CommandDispatcher::check(const CommandSource& source,
                                                           const std::string& input) const {
    StringReader reader{input};
    const DeferredSource deferred{source};
    const Parsed<std::vector<Link>> links = parse(reader, root_, deferred, {});
    if (!links.has_value()) {
        return links.error();
    }
    if (links->empty() || !links->back().executor) {
        return reader.syntaxError(std::string{kUnknownCommand});
    }
    // parity: a function is compiled with its permission level, so a command above it does not parse.
    const std::string name = lowered(firstWord(input));
    for (const std::unique_ptr<CommandNode>& child : root_.children()) {
        if (child->name() == name && child->requiredLevel > source.permissionLevel()) {
            StringReader start{input};
            return start.syntaxError(std::string{kUnknownCommand});
        }
    }
    return std::nullopt;
}

std::vector<std::pair<std::string, std::string>> CommandDispatcher::signableArguments(const CommandSource& source,
                                                                                      const std::string& input) const {
    StringReader reader{input};
    const Parsed<std::vector<Link>> links = parse(reader, root_, source, {});
    std::vector<std::pair<std::string, std::string>> found;
    if (!links.has_value()) {
        return found;
    }
    for (const Link& link : *links) {
        for (const auto& [name, value] : link.arguments) {
            const std::string* text = value.signable() ? value.as<std::string>() : nullptr;
            const bool seen = std::ranges::any_of(found, [&name](const auto& entry) { return entry.first == name; });
            if (text != nullptr && !seen) {
                found.emplace_back(name, *text);
            }
        }
    }
    return found;
}

Parsed<std::int32_t> CommandDispatcher::runChain(const std::vector<Link>& links, std::size_t index,
                                                 const CommandSource& source, const std::string& input,
                                                 StringReader& reader, std::int32_t& successes) const {
    const Link& link = links[index];
    if (index + 1 == links.size()) {
        if (!link.executor) {
            return std::unexpected{reader.syntaxError(std::string{kUnknownCommand})};
        }
        resultDeferred = false;
        const Parsed<std::int32_t> result = link.executor(CommandContext{source, *this, input, link.arguments});
        const bool deferred = std::exchange(resultDeferred, false);
        // parity: brigadier tells its consumer `true` for a leaf that ran at all.
        if (result.has_value()) {
            ++successes;
        }
        const ExecutionSource* execution = source.asExecution();
        if (execution != nullptr && !deferred) {
            execution->report(result.has_value() ? *result : 0);
        }
        return result;
    }
    if (!link.modifier) {
        return runChain(links, index + 1, source, input, reader, successes);
    }
    CommandContext modifierContext{source, *this, input, link.arguments};
    modifierContext.setTail(link.node, link.cursor);
    const std::vector<std::shared_ptr<const CommandSource>> sources = link.modifier(modifierContext);
    if (!link.forks) {
        if (sources.size() != 1) {
            return 0;
        }
        return runTail(link, *sources.front(), input, successes);
    }
    // parity: `max_command_forks`, which stops a fork that would hand on that many sources.
    const world::LoadedWorld* world = worldIn(source, *environment_);
    const std::int32_t forkLimit = world == nullptr ? std::numeric_limits<std::int32_t>::max()
                                                    : world->gameRules().number(world::gameRules::kMaxCommandForks);
    if (std::cmp_greater_equal(sources.size(), forkLimit)) {
        return std::unexpected{
            CommandSyntaxError{.component = wire::text::Component::translatable(
                                   "command.forkLimit", {wire::text::Component::text(std::to_string(forkLimit))})}};
    }
    std::int32_t worked = 0;
    for (const std::shared_ptr<const CommandSource>& next : sources) {
        // One branch failing is not the others' problem: vanilla runs the rest and counts.
        const Parsed<std::int32_t> result = runTail(link, *next, input, successes);
        if (result.has_value() && *result > 0) {
            worked++;
        }
    }
    return worked;
}

void CommandDispatcher::deferResult() { resultDeferred = true; }

std::int32_t CommandDispatcher::continueTail(const CommandNode& node, std::int32_t cursor, const std::string& input,
                                             const CommandSource& source, std::uint64_t generation) const {
    if (generation != generation_) {
        return 0;
    }
    std::int32_t successes = 0;
    const Link link{.arguments = {}, .modifier = {}, .forks = false, .executor = {}, .node = &node, .cursor = cursor};
    const Parsed<std::int32_t> ran = runTail(link, source, input, successes);
    if (!ran.has_value()) {
        reportSyntaxError(source, ran.error(), input);
        return 0;
    }
    return *ran;
}

Parsed<std::int32_t> CommandDispatcher::runTail(const Link& link, const CommandSource& source, const std::string& input,
                                                std::int32_t& successes) const {
    // The tail was parsed for whoever typed the line; `@s` and `~` mean the source it reaches now.
    StringReader reader{input};
    reader.setCursor(link.cursor);
    const Parsed<std::vector<Link>> reparsed = parse(reader, *link.node, source, {}, true);
    if (!reparsed.has_value()) {
        return std::unexpected{reparsed.error()};
    }
    return runChain(*reparsed, 1, source, input, reader, successes);
}

Parsed<std::vector<CommandDispatcher::Link>> CommandDispatcher::parse(StringReader& reader, const CommandNode& node,
                                                                      const CommandSource& typist,
                                                                      const Arguments& arguments,
                                                                      const bool resolveTail) const {
    CommandNode* redirect = node.redirect;
    const std::int32_t entry = reader.cursor();
    // Past a modifier the line is only checked for shape: it runs, and resolves, once per source.
    const DeferredSource deferred{typist};
    const CommandSource& source = redirect != nullptr && node.modifier && !resolveTail ? deferred : typist;
    if (!reader.canRead()) {
        return std::vector<Link>{Link{.arguments = arguments, .executor = node.executor}};
    }
    if (node.kind() != NodeKind::Root) {
        const Parsed<void> separator = reader.expect(' ');
        if (!separator.has_value()) {
            return std::unexpected{separator.error()};
        }
    }
    if (!reader.canRead()) {
        return std::unexpected{reader.syntaxError(std::string{kUnknownCommand})};
    }

    const CommandNode& target = redirect == nullptr ? node : *redirect;
    const std::int32_t start = reader.cursor();
    std::optional<CommandSyntaxError> deepest;
    for (CommandNode* child : target.sortedChildren()) {
        if (!mayUse(source, *child)) {
            continue;
        }
        reader.setCursor(start);
        Arguments branch = redirect == nullptr ? arguments : Arguments{};
        std::optional<CommandSyntaxError> failure;
        if (child->kind() == NodeKind::Root) {
            continue;
        }
        if (child->kind() == NodeKind::Literal) {
            if (!reader.readLiteral(child->name())) {
                continue;
            }
        } else {
            const Parsed<ArgumentValue> value = child->type()->parse(reader, source, *environment_);
            if (!value.has_value()) {
                failure = value.error();
            } else {
                ArgumentValue parsed = *value;
                if (child->type()->signable()) {
                    parsed.markSignable();
                }
                branch.insert_or_assign(child->name(), std::move(parsed));
            }
        }
        if (!failure.has_value() && reader.canRead() && reader.peek() != ' ') {
            failure = reader.syntaxError(std::string{kExpectedSeparator});
        }
        if (!failure.has_value()) {
            const Parsed<std::vector<Link>> tail = parse(reader, *child, source, branch);
            if (tail.has_value()) {
                if (redirect == nullptr) {
                    return tail;
                }
                std::vector<Link> chained{Link{.arguments = arguments,
                                               .modifier = node.modifier,
                                               .forks = node.forks,
                                               .node = &node,
                                               .cursor = entry}};
                chained.insert(chained.end(), tail->begin(), tail->end());
                return chained;
            }
            failure = tail.error();
        }
        if (!deepest.has_value() || failure->cursor > deepest->cursor) {
            deepest = failure;
        }
    }
    reader.setCursor(start);
    return std::unexpected{deepest.has_value() ? *deepest : reader.syntaxError(std::string{kUnknownCommand})};
}

SuggestionResult CommandDispatcher::suggest(const CommandSource& source, const std::string& input) const {
    const std::string line = input.starts_with('/') ? input.substr(1) : input;
    const std::int32_t offset = static_cast<std::int32_t>(input.size() - line.size());
    std::vector<std::pair<std::int32_t, CommandSuggestion>> collected;
    StringReader reader{line};
    const Arguments empty;
    collect(reader, root_, source, empty, collected);
    if (collected.empty()) {
        return SuggestionResult{};
    }
    std::int32_t start = collected.front().first;
    for (const auto& [at, suggestion] : collected) {
        start = std::max(start, at);
    }
    std::vector<CommandSuggestion> suggestions;
    std::set<std::string, std::less<>> seen;
    for (const auto& [at, suggestion] : collected) {
        if (at == start && seen.insert(suggestion.text).second) {
            suggestions.push_back(suggestion);
        }
    }
    std::ranges::sort(suggestions, [](const CommandSuggestion& left, const CommandSuggestion& right) {
        return left.text < right.text;
    });
    return SuggestionResult{.start = start + offset,
                            .length = static_cast<std::int32_t>(line.size()) - start,
                            .suggestions = std::move(suggestions)};
}

void CommandDispatcher::collect(StringReader& reader, const CommandNode& node, const CommandSource& source,
                                const Arguments& arguments,
                                std::vector<std::pair<std::int32_t, CommandSuggestion>>& out) const {
    const CommandNode& target = node.redirect == nullptr ? node : *node.redirect;
    if (node.kind() != NodeKind::Root) {
        if (!reader.canRead() || reader.peek() != ' ') {
            return;
        }
        reader.skip();
    }
    const std::int32_t start = reader.cursor();
    const std::string rest = reader.string().substr(static_cast<std::size_t>(start));
    const bool typing = rest.find(' ') == std::string::npos;

    for (const CommandNode* child : target.sortedChildren()) {
        if (!mayUse(source, *child)) {
            continue;
        }
        reader.setCursor(start);
        if (child->kind() == NodeKind::Root) {
            continue;
        }
        if (child->kind() == NodeKind::Literal) {
            if (typing && lowered(child->name()).starts_with(lowered(rest))) {
                out.emplace_back(start, CommandSuggestion{.text = child->name()});
            }
            if (!reader.readLiteral(child->name())) {
                continue;
            }
            collect(reader, *child, source, arguments, out);
            continue;
        }
        if (typing) {
            if (const SuggestionProvider provider = child->provider(); provider) {
                for (CommandSuggestion& suggestion : provider(SuggestionContext{source, *this, rest, arguments})) {
                    out.emplace_back(start, std::move(suggestion));
                }
            }
        }
        reader.setCursor(start);
        const Parsed<ArgumentValue> value = child->type()->parse(reader, source, *environment_);
        if (!value.has_value() || (reader.canRead() && reader.peek() != ' ')) {
            continue;
        }
        Arguments deeper = arguments;
        deeper.insert_or_assign(child->name(), *value);
        collect(reader, *child, source, deeper, out);
    }
}

void CommandDispatcher::reportSyntaxError(const CommandSource& source, const CommandSyntaxError& error,
                                          const std::string& input) const {
    feedback_.failure(source, error.component);
    if (!error.input.has_value() || error.cursor < 0) {
        return;
    }
    const std::string& rawInput = *error.input;
    const std::size_t cursor = std::min(rawInput.size(), static_cast<std::size_t>(error.cursor));
    std::vector<Component> context;
    if (static_cast<std::int32_t>(cursor) > kContextCharacters) {
        context.push_back(Component::text("..."));
    }
    const std::size_t from = cursor > static_cast<std::size_t>(kContextCharacters)
                                 ? cursor - static_cast<std::size_t>(kContextCharacters)
                                 : 0;
    context.push_back(Component::text(rawInput.substr(from, cursor - from)));
    if (cursor < rawInput.size()) {
        context.push_back(Component::text(rawInput.substr(cursor)).color(wire::text::kRed).underlined(true));
    }
    context.push_back(Component::translatable("command.context.here").color(wire::text::kRed).italic(true));
    // parity: `Commands.finishParsing`.
    const Component line = Component::text("")
                               .withChildren(std::move(context))
                               .color(wire::text::kGray)
                               .clickEvent(wire::text::ClickEvent::suggestCommand("/" + input));
    source.sendSystemMessage(Component::text("").withChildren({line}).color(wire::text::kRed));
}

CommandsPacket CommandDispatcher::treeFor(const CommandSource& source) const {
    std::map<const CommandNode*, std::int32_t> ids;
    std::vector<const CommandNode*> ordered;

    const std::function<void(const CommandNode&)> enumerate = [&](const CommandNode& node) {
        if (ids.contains(&node)) {
            return;
        }
        ids.emplace(&node, static_cast<std::int32_t>(ordered.size()));
        ordered.push_back(&node);
        for (const CommandNode* child : node.sortedChildren()) {
            if (mayUse(source, *child)) {
                enumerate(*child);
            }
        }
        if (node.redirect != nullptr) {
            enumerate(*node.redirect);
        }
    };
    enumerate(root_);

    std::vector<CommandNodeData> nodes;
    nodes.reserve(ordered.size());
    for (const CommandNode* node : ordered) {
        std::vector<std::int32_t> children;
        for (const CommandNode* child : node->sortedChildren()) {
            if (!mayUse(source, *child)) {
                continue;
            }
            if (const auto found = ids.find(child); found != ids.end()) {
                children.push_back(found->second);
            }
        }
        std::optional<std::int32_t> redirect;
        if (node->redirect != nullptr) {
            if (const auto found = ids.find(node->redirect); found != ids.end()) {
                redirect = found->second;
            }
        }
        if (node->kind() == NodeKind::Root) {
            nodes.push_back(CommandNodeData{
                .kind = CommandNodeData::Kind::Root, .executable = false, .children = std::move(children)});
            continue;
        }
        if (node->kind() == NodeKind::Literal) {
            nodes.push_back(CommandNodeData{.kind = CommandNodeData::Kind::Literal,
                                            .executable = static_cast<bool>(node->executor),
                                            .children = std::move(children),
                                            .redirect = redirect,
                                            .name = node->name()});
            continue;
        }
        nodes.push_back(CommandNodeData{.kind = CommandNodeData::Kind::Argument,
                                        .executable = static_cast<bool>(node->executor),
                                        .children = std::move(children),
                                        .redirect = redirect,
                                        .name = node->name(),
                                        .parser = node->type()->wire(),
                                        .suggestionId = node->suggestionId()});
    }
    return CommandsPacket{.nodes = std::move(nodes), .rootIndex = 0};
}

std::vector<std::pair<std::string, std::string>> CommandDispatcher::unreachableSuggestions() const {
    std::vector<std::pair<std::string, std::string>> found;
    std::set<const CommandNode*> seen;
    const std::function<void(const CommandNode&, const std::string&)> walk = [&](const CommandNode& node,
                                                                                 const std::string& path) {
        if (!seen.insert(&node).second) {
            return;
        }
        for (const std::unique_ptr<CommandNode>& child : node.children()) {
            const std::string name = path.empty() ? child->name() : path + " " + child->name();
            if (child->kind() == NodeKind::Argument && child->provider() && !child->answersOnItsOwn()) {
                found.emplace_back(name, std::to_string(child->type()->wire().registryId()));
            }
            walk(*child, name);
        }
    };
    walk(root_, "");
    return found;
}

std::vector<CommandNode*> CommandDispatcher::usableCommands(const CommandSource& source) const {
    std::vector<CommandNode*> usable;
    for (const std::unique_ptr<CommandNode>& child : root_.children()) {
        if (child->kind() == NodeKind::Literal && mayUse(source, *child)) {
            usable.push_back(child.get());
        }
    }
    return usable;
}

namespace {

const CommandNode* deepestIn(StringReader& reader, const CommandNode& node, const CommandSource& source,
                             const CommandEnvironment& environment, std::int32_t& reached) {
    const CommandNode& target = node.redirect == nullptr ? node : *node.redirect;
    if (node.kind() != NodeKind::Root) {
        if (!reader.canRead() || reader.peek() != ' ') {
            reached = reader.cursor();
            return nullptr;
        }
        reader.skip();
    }
    const std::int32_t start = reader.cursor();
    const CommandNode* best = nullptr;
    std::int32_t bestAt = start;
    for (CommandNode* child : target.sortedChildren()) {
        if (!mayUse(source, *child)) {
            continue;
        }
        reader.setCursor(start);
        bool matched = false;
        if (child->kind() == NodeKind::Literal) {
            matched = reader.readLiteral(child->name());
        } else if (child->kind() == NodeKind::Argument) {
            matched = child->type()->parse(reader, source, environment).has_value();
        }
        if (!matched) {
            continue;
        }
        const CommandNode* candidate = child;
        std::int32_t at = reader.cursor();
        if (reader.canRead() && reader.peek() == ' ') {
            std::int32_t deeperAt = at;
            if (const CommandNode* deeper = deepestIn(reader, *child, source, environment, deeperAt);
                deeper != nullptr) {
                candidate = deeper;
                at = deeperAt;
            }
        }
        if (best == nullptr || at > bestAt) {
            best = candidate;
            bestAt = at;
        }
    }
    reached = best == nullptr ? start : bestAt;
    reader.setCursor(reached);
    return best;
}

} // namespace

const CommandNode* CommandDispatcher::deepestParsed(const CommandSource& source, const std::string& input) const {
    StringReader reader{input};
    std::int32_t reached = 0;
    return deepestIn(reader, root_, source, *environment_, reached);
}

} // namespace basalt::server::runtime::command
