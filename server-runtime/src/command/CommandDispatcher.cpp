#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <map>
#include <set>
#include <utility>

namespace basalt::server::runtime::command {
namespace {

/// The source a line is parsed against past an `/execute` modifier. Everything is the typist's; it
/// only lets a selector come up empty, since the sources that will really run are not known yet.
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

private:
    const CommandSource* typist_;
};

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

/// The first word of a line, which is what a timing is named after.
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
    // parity: `CommandSourceStack.getLevel`, which is the world `/execute in` chose when it chose one.
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

Command::Command(std::string name, std::int32_t level, std::vector<std::string> aliases,
                 std::optional<std::string> permission)
    : name_(std::move(name)), level_(level), aliases_(std::move(aliases)),
      permission_(permission.has_value() ? std::move(*permission) : "basalt.command." + name_) {}

CommandDispatcher::CommandDispatcher(const CommandEnvironment& environment)
    : environment_(&environment), root_(NodeKind::Root, ""), feedback_(environment) {
    registerCustom();
}

void CommandDispatcher::registerCustom() {
    // The operator's own commands are built by `ConfiguredCommand`, which arrives with the rest of
    // the command bodies. Until then the file's custom section declares nothing that can be built.
}

CommandNode* CommandDispatcher::registerCommand(std::shared_ptr<const Command> command) {
    // Remembered before the override is consulted, so a command switched off in the file can be
    // switched back on by a later reload.
    const auto same = std::ranges::find_if(registered_, [&command](const std::shared_ptr<const Command>& held) {
        return held->name() == command->name();
    });
    const Command& definition = *command;
    // The one that arrives last is the one the tree is built from, so it is the one that has to be
    // held: the nodes keep references into the command object, and dropping it leaves the tree
    // reading freed memory the next time somebody runs it.
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
    // `registerCommand` keeps the old one alive because the nodes still reference it.
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
    // Before build(), so children inherit them: a sub-command is never easier to reach than the
    // command it belongs to.
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
    // Named after the command, so a report shows which one is slow rather than that "commands" are.
    // The name comes from the input rather than the parse, since a command that fails to parse
    // still cost whatever it cost.
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
        const Parsed<std::int32_t> result = link.executor(CommandContext{source, *this, input, link.arguments});
        // parity: brigadier tells its consumer `true` for a leaf that ran at all, whatever it
        // answered, and `false` only for one that threw. That is what a command block counts.
        if (result.has_value()) {
            ++successes;
        }
        const ExecutionSource* execution = source.asExecution();
        if (execution != nullptr) {
            execution->report(result.has_value() ? *result : 0);
        }
        return result;
    }
    if (!link.modifier) {
        return runChain(links, index + 1, source, input, reader, successes);
    }
    const std::vector<std::shared_ptr<const CommandSource>> sources =
        link.modifier(CommandContext{source, *this, input, link.arguments});
    if (!link.forks) {
        if (sources.size() != 1) {
            return 0;
        }
        return runTail(link, *sources.front(), input, successes);
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
    // Every node but the first is preceded by the separator that ended the previous one.
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
        // What follows a redirect is its own command, so it starts with no arguments.
        Arguments branch = redirect == nullptr ? arguments : Arguments{};
        std::optional<CommandSyntaxError> failure;
        if (child->kind() == NodeKind::Root) {
            continue;
        }
        if (child->kind() == NodeKind::Literal) {
            // Matched as text, not as a word: `/execute if score ... >= ...` names literals that
            // are not words at all. parity: `LiteralCommandNode.parse`.
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
    // The deepest token wins: a branch that got further is completing a more specific word than one
    // that stopped earlier, and a single reply carries a single range.
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
    // A word still being typed has nothing after it: anything with a space is already settled, and
    // the cursor is further along the line.
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
    // parity: `Commands.finishParsing`, whose line suggests the whole command again when clicked
    // and goes out through `sendFailure`, so inside a red parent.
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
                // The parser by its registry number, which is what a report has to name it by: the
                // wire carries an id and nothing else.
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

} // namespace basalt::server::runtime::command
