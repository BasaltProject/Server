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

/// Result reporting, shared by every command so the surrounding behaviour (who else sees it, what
/// reaches the log) is decided once rather than per command.
///
/// Mirrors `CommandSourceStack.sendSuccess`/`sendFailure`: a successful command with
/// `broadcastToAdmins` set also reaches every *other* operator as `chat.type.admin`, and the
/// console log, which is vanilla's `logAdminCommands` game rule. A failure is red and goes only to
/// whoever ran it.
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

/// Everything one running command can see: who ran it, its parsed arguments, and the server around
/// it.
///
/// Values are stored already parsed, so a command body reads `context.get<std::int32_t>("count")`
/// rather than re-parsing text. Asking for a name the tree never declared answers null, which is a
/// bug in the command rather than in what the player typed.
///
/// This is the whole surface a command is written against, which is why reporting and the two
/// "there has to be a player here" checks live on it rather than being passed around.
class CommandContext {
public:
    CommandContext(const CommandSource& source, const CommandDispatcher& dispatcher, std::string input,
                   Arguments arguments);

    [[nodiscard]] const CommandSource& source() const noexcept { return *source_; }

    [[nodiscard]] const CommandDispatcher& dispatcher() const noexcept { return *dispatcher_; }

    [[nodiscard]] const CommandEnvironment& environment() const;

    [[nodiscard]] const std::string& input() const noexcept { return input_; }

    [[nodiscard]] const Arguments& arguments() const noexcept { return arguments_; }

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
};

/// One command, as a class: its name, the permission it needs, its aliases, and the tree it builds.
///
/// Vanilla gives every command its own class under `net.minecraft.server.commands` and this follows
/// that split, one file each, for the same reason: a command is a self-contained unit of behaviour
/// and reads better next to nothing else. It is also the shape plugins need later, since a plugin
/// contributing a command should have exactly this to fill in and nothing more.
///
/// Aliases are registered as redirects onto this command's own node, not as copies, so `/tp` really
/// is `/teleport` rather than a second tree that can drift from it.
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
///
/// parity: brigadier's `CommandDispatcher.getSmartUsage`, including its bracket rules: a branch
/// whose parent is already executable is optional and gets `[...]`, one that is not gets `(...)`,
/// and several alternatives are joined with `|`. Vanilla's `/help` output is exactly these strings
/// with a slash in front.
[[nodiscard]] std::string smartUsage(const CommandNode& node, const CommandSource& source, bool optional = false,
                                     bool deep = false);

/// Parses and runs command lines, and hands out the tree the client needs to parse them itself.
///
/// Failure handling follows vanilla's `Commands.performCommand`: the parse error goes back to the
/// source, followed by the grey "context" line marking where the parser stopped
/// (`command.context.here`, rendered by the client as `<--[HERE]`). A command that fails reports
/// `command.failed` instead, and never takes the connection down with it.
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

    /// Builds the tree again from the current `commands.toml`, whole rather than patched:
    /// permission, aliases and existence are all decided during registration, so diffing would mean
    /// writing registration a second way. A few dozen nodes cost nothing and cannot drift from what
    /// a fresh start produces.
    ///
    /// The caller sends the new tree to connected clients; until then they autocomplete the old one.
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
    std::int32_t execute(const CommandSource& source, const std::string& input);

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
    };

    void registerCustom();

    CommandNode* registerBuilt(const Command& command);

    [[nodiscard]] Parsed<std::vector<Link>> parse(StringReader& reader, CommandNode& node, const CommandSource& source,
                                                  const Arguments& arguments) const;

    [[nodiscard]] Parsed<std::int32_t> runChain(const std::vector<Link>& links, std::size_t index,
                                                const CommandSource& source, const std::string& input,
                                                StringReader& reader) const;

    void collect(StringReader& reader, const CommandNode& node, const CommandSource& source, const Arguments& arguments,
                 std::vector<std::pair<std::int32_t, wire::protocol::play::CommandSuggestion>>& out) const;

    void reportSyntaxError(const CommandSource& source, const CommandSyntaxError& error) const;

    const CommandEnvironment* environment_;
    CommandNode root_;
    CommandFeedback feedback_;
    /// What was registered from code, so a reload can build the tree again from the same list.
    std::vector<std::shared_ptr<const Command>> registered_;
};

} // namespace basalt::server::runtime::command
