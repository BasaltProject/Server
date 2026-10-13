#pragma once

#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/command/CommandTree.hpp>
#include <basalt/server/runtime/command/MessageOverrides.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::command {

/// Result reporting, shared by every command. parity: `CommandSourceStack.sendSuccess`/`sendFailure`:
/// a success with `broadcastToAdmins` also reaches every *other* operator as `chat.type.admin` and the
/// console log (`logAdminCommands`); a failure is red and goes only to whoever ran it.
class CommandFeedback {
public:
    explicit CommandFeedback(const CommandEnvironment& environment);

    void success(const CommandSource& source, const wire::text::Component& rawMessage, bool broadcastToAdmins) const;

    void failure(const CommandSource& source, const wire::text::Component& message) const;

private:
    const CommandEnvironment* environment_;
    MessageOverrides overrides_;
};

class CommandDispatcher;

/// Everything one running command can see: who ran it, its parsed arguments, and the server around it.
/// Values are stored parsed (`context.get<std::int32_t>("count")`); a name the tree never declared
/// answers null, which is a bug in the command rather than in the input.
class CommandContext {
public:
    CommandContext(const CommandSource& source, const CommandDispatcher& dispatcher, std::string input,
                   Arguments arguments);

    [[nodiscard]] const CommandSource& source() const noexcept { return *source_; }

    [[nodiscard]] const CommandDispatcher& dispatcher() const noexcept { return *dispatcher_; }

    [[nodiscard]] const CommandEnvironment& environment() const;

    [[nodiscard]] const std::string& input() const noexcept { return input_; }

    [[nodiscard]] const Arguments& arguments() const noexcept { return arguments_; }

    /// What follows a redirect: the node its tail is parsed from and where, for a modifier that
    /// continues the line later. Null outside a modifier.
    [[nodiscard]] const CommandNode* tailNode() const noexcept { return tailNode_; }

    [[nodiscard]] std::int32_t tailCursor() const noexcept { return tailCursor_; }

    void setTail(const CommandNode* node, std::int32_t cursor) {
        tailNode_ = node;
        tailCursor_ = cursor;
    }

    [[nodiscard]] bool has(std::string_view name) const;

    template <typename T>
    [[nodiscard]] const T* get(std::string_view name) const {
        const auto found = arguments_.find(name);
        return found == arguments_.end() ? nullptr : found->second.as<T>();
    }

    /// Reports success, and with `broadcastToAdmins` set also tells every other operator and the
    /// log. Mirrors `CommandSourceStack.sendSuccess`, whose boolean means the same thing.
    void success(const wire::text::Component& message, bool broadcastToAdmins = false) const;

    /// parity: `CommandSourceStack.getPlayerOrException`, for forms that act on "you" as a player.
    [[nodiscard]] Parsed<player::PlayerSession*> requirePlayer() const;

    /// parity: `CommandSourceStack.getEntityOrException`, for forms that act on "you" as an entity.
    [[nodiscard]] Parsed<player::PlayerSession*> requireEntity() const;

    /// The world this command acts on: the one its runner is standing in, or the default for the
    /// console.
    [[nodiscard]] Parsed<world::LoadedWorld*> world() const;

private:
    const CommandSource* source_;
    const CommandDispatcher* dispatcher_;
    std::string input_;
    Arguments arguments_;
    const CommandNode* tailNode_ = nullptr;
    std::int32_t tailCursor_ = 0;
};

/// One command, as a class: its name, the permission it needs, its aliases, and the tree it builds.
/// One file each, like vanilla's `net.minecraft.server.commands`, and the shape a plugin fills in.
/// Aliases are redirects onto this command's node, not copies, so `/tp` cannot drift from `/teleport`.
class Command {
public:
    Command(std::string name, std::int32_t level = permissionLevel::kAll, std::vector<std::string> aliases = {},
            std::optional<std::string> permission = std::nullopt);

    Command(const Command&) = default;
    Command& operator=(const Command&) = default;
    Command(Command&&) = default;
    Command& operator=(Command&&) = default;
    virtual ~Command() = default;

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    [[nodiscard]] std::int32_t permissionLevel() const noexcept { return level_; }

    [[nodiscard]] const std::vector<std::string>& aliases() const noexcept { return aliases_; }

    /// Defaults to `basalt.command.<name>`, which is what `groups.toml` grants by wildcard.
    [[nodiscard]] const std::string& permission() const noexcept { return permission_; }

    /// Builds this command's arguments and sub-commands onto its own (already named) root node.
    virtual void build(const CommandNodeBuilder& root) const = 0;

private:
    std::string name_;
    std::int32_t level_;
    std::vector<std::string> aliases_;
    std::string permission_;
};

/// The one-line syntax summary `/help` prints, e.g. `gamemode <gamemode> [<target>]`.
/// parity: brigadier's `CommandDispatcher.getSmartUsage`: a branch under an executable parent gets
/// `[...]`, otherwise `(...)`, and alternatives are joined with `|`.
[[nodiscard]] std::string smartUsage(const CommandNode& node, const CommandSource& source, bool optional = false,
                                     bool deep = false);

/// The usage lines `/help <command>` prints for the node a line reached: one per usable child.
/// parity: brigadier's `CommandDispatcher.getSmartUsage` map form.
[[nodiscard]] std::vector<std::string> smartUsages(const CommandNode& node, const CommandSource& source);

/// Parses and runs command lines, and hands out the tree the client needs to parse them itself.
/// parity: `Commands.performCommand`: a parse error is followed by the grey `command.context.here`
/// line; a command that fails reports `command.failed` and never takes the connection down.
class CommandDispatcher {
public:
    /// Brigadier's own literals, which the client shows verbatim: they are not translation keys.
    static constexpr std::string_view kUnknownCommand = "Unknown command";
    static constexpr std::string_view kExpectedSeparator =
        "Expected whitespace to end one argument, but found trailing data";

    /// How much of the line vanilla echoes back before the `<--[HERE]` marker.
    static constexpr std::int32_t kContextCharacters = 10;

    explicit CommandDispatcher(const CommandEnvironment& environment);

    [[nodiscard]] const CommandEnvironment& environment() const noexcept { return *environment_; }

    [[nodiscard]] const CommandFeedback& feedback() const noexcept { return feedback_; }

    /// The tree's root: what `/execute run` and every alias redirect onto.
    [[nodiscard]] CommandNode& rootNode() noexcept { return root_; }

    [[nodiscard]] const CommandNode& rootNode() const noexcept { return root_; }

    /// Builds the tree again from the current `commands.toml`, whole rather than patched: permission,
    /// aliases and existence are decided at registration, so a rebuild cannot drift from a fresh start.
    /// The caller sends the new tree to clients; until then they autocomplete the old one.
    void rebuild();

    /// Registers a command and every alias it declares, as `commands.toml` says to.
    ///
    /// Answers null for a command that file switched off. Off means gone: it is not registered, so
    /// it is not in the tree sent to clients either, and it fails as an unknown command rather than
    /// as a refusal. A command that answers "you may not do that" tells everyone it exists.
    CommandNode* registerCommand(std::shared_ptr<const Command> command);

    /// The lower-level form, for a tree built inline rather than by a `Command`.
    CommandNode& registerNode(std::string name, const std::function<void(const CommandNodeBuilder&)>& build);

    /// Takes a node back out of the tree.
    ///
    /// For what is registered while the server runs and can go away again, which today means a
    /// plugin's commands: unloading one has to leave the tree as it was, aliases included.
    void unregisterNode(const CommandNode* node);

    /// Registers a name that redirects onto a target, the way vanilla declares `/tp` and `/xp`.
    CommandNode& registerAlias(std::string name, CommandNode& target);

    /// Runs a line (without its leading slash) as a source. Answers the command's own result, or 0
    /// if it failed; the failure has already been reported to the source by then.
    ///
    /// Const because running a command reads the tree rather than changing it, which is what lets a
    /// command re-enter the dispatcher: `/placeholder cmdparse` and `/execute run` both do.
    std::int32_t execute(const CommandSource& source, const std::string& input) const;

    /// The same, also counting how many times a command ran without failing. parity: the count is not
    /// the result: `/time query gametime` succeeds once and answers the clock, `/execute as @e` over
    /// three entities succeeds three times. A comparator on a command block reads the count.
    std::int32_t execute(const CommandSource& source, const std::string& input, std::int32_t& successes) const;

    /// Parses a line without running it, answering why it does not parse. parity: how
    /// `CommandFunction.fromLines` compiles a function's lines with its compilation source.
    [[nodiscard]] std::optional<CommandSyntaxError> check(const CommandSource& source, const std::string& input) const;

    /// Runs what follows a redirect for one source, later than the line it was on. parity:
    /// `BuildContexts.Continuation`, what `execute if function` resumes with. Nothing runs when the
    /// tree was rebuilt since `generation`.
    std::int32_t continueTail(const CommandNode& node, std::int32_t cursor, const std::string& input,
                              const CommandSource& source, std::uint64_t generation) const;

    /// Bumped whenever the tree changes, so a held node can be checked before it is used.
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }

    /// Tells the dispatcher the command that is running answers later, so the stores waiting on
    /// this line are not told now. What `/function` does when it queues.
    static void deferResult();

    /// parity: `SignableCommand.of`: the message arguments of a line, by node name, with the text a
    /// client signs for each. Empty for a line that does not parse.
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> signableArguments(const CommandSource& source,
                                                                                     const std::string& input) const;

    /// What a line could be completed with, the line ending where the client's cursor is.
    [[nodiscard]] SuggestionResult suggest(const CommandSource& source, const std::string& input) const;

    /// The tree a source may use, flattened for `CommandsPacket`. Filtered by permission exactly as
    /// vanilla's `Commands.fillUsableCommands` filters by `canUse`: a player who cannot run
    /// `/gamemode` is never told it exists, so their client greys it out rather than letting them
    /// type it and be refused.
    [[nodiscard]] wire::protocol::play::CommandsPacket treeFor(const CommandSource& source) const;

    /// Arguments that can answer a completion nobody will ever ask them for: no provider name in the
    /// tree, and a parser the client cannot complete from either. Empty is the invariant; a test
    /// keeps it that way.
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> unreachableSuggestions() const;

    /// Every registered top-level command name, in registration order. Used by `/help`.
    [[nodiscard]] std::vector<CommandNode*> usableCommands(const CommandSource& source) const;

    /// The deepest node a line parses to for `source`, or null when nothing of it parses. What
    /// `/help <line>` describes; the line is never run.
    [[nodiscard]] const CommandNode* deepestParsed(const CommandSource& source, const std::string& input) const;

private:
    /// One link of a parsed line: what it parsed, and what it does to the source for the next link.
    ///
    /// parity: brigadier's `CommandContext` chain. A redirect ends a link, which is why
    /// `/execute as @e run say hi` keeps `@e` instead of losing it to the last redirect.
    struct Link {
        Arguments arguments;
        SourceModifier modifier;
        bool forks = false;
        CommandExecutor executor;
        /// Where a modifier link was parsed from, so what follows it is parsed again for every
        /// source it hands on.
        const CommandNode* node = nullptr;
        std::int32_t cursor = 0;
    };

    void registerCustom();

    CommandNode* registerBuilt(const Command& command);

    /// `resolveTail` parses what follows this node's modifier for `source` itself rather than as a
    /// stand-in, which is what running a tail again for one of its sources needs.
    [[nodiscard]] Parsed<std::vector<Link>> parse(StringReader& reader, const CommandNode& node,
                                                  const CommandSource& source, const Arguments& arguments,
                                                  bool resolveTail = false) const;

    [[nodiscard]] Parsed<std::int32_t> runChain(const std::vector<Link>& links, std::size_t index,
                                                const CommandSource& source, const std::string& input,
                                                StringReader& reader, std::int32_t& successes) const;

    [[nodiscard]] Parsed<std::int32_t> runTail(const Link& link, const CommandSource& source, const std::string& input,
                                               std::int32_t& successes) const;

    void collect(StringReader& reader, const CommandNode& node, const CommandSource& source, const Arguments& arguments,
                 std::vector<std::pair<std::int32_t, wire::protocol::play::CommandSuggestion>>& out) const;

    void reportSyntaxError(const CommandSource& source, const CommandSyntaxError& error,
                           const std::string& input) const;

    const CommandEnvironment* environment_;
    CommandNode root_;
    CommandFeedback feedback_;
    std::uint64_t generation_ = 0;
    /// What was registered from code, so a reload can build the tree again from the same list.
    std::vector<std::shared_ptr<const Command>> registered_;
};

} // namespace basalt::server::runtime::command
