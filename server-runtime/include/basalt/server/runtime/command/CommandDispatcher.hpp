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

/// parity: `CommandSourceStack.sendSuccess`/`sendFailure`.
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

class CommandContext {
public:
    CommandContext(const CommandSource& source, const CommandDispatcher& dispatcher, std::string input,
                   Arguments arguments);

    [[nodiscard]] const CommandSource& source() const noexcept { return *source_; }

    [[nodiscard]] const CommandDispatcher& dispatcher() const noexcept { return *dispatcher_; }

    [[nodiscard]] const CommandEnvironment& environment() const;

    [[nodiscard]] const std::string& input() const noexcept { return input_; }

    [[nodiscard]] const Arguments& arguments() const noexcept { return arguments_; }

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

    void success(const wire::text::Component& message, bool broadcastToAdmins = false) const;

    /// parity: `CommandSourceStack.getPlayerOrException`.
    [[nodiscard]] Parsed<player::PlayerSession*> requirePlayer() const;

    /// parity: `CommandSourceStack.getEntityOrException`.
    [[nodiscard]] Parsed<player::PlayerSession*> requireEntity() const;

    [[nodiscard]] Parsed<world::LoadedWorld*> world() const;

private:
    const CommandSource* source_;
    const CommandDispatcher* dispatcher_;
    std::string input_;
    Arguments arguments_;
    const CommandNode* tailNode_ = nullptr;
    std::int32_t tailCursor_ = 0;
};

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

    [[nodiscard]] const std::string& permission() const noexcept { return permission_; }

    virtual void build(const CommandNodeBuilder& root) const = 0;

private:
    std::string name_;
    std::int32_t level_;
    std::vector<std::string> aliases_;
    std::string permission_;
};

/// parity: brigadier's `CommandDispatcher.getSmartUsage`.
[[nodiscard]] std::string smartUsage(const CommandNode& node, const CommandSource& source, bool optional = false,
                                     bool deep = false);

/// parity: brigadier's `CommandDispatcher.getSmartUsage` map form.
[[nodiscard]] std::vector<std::string> smartUsages(const CommandNode& node, const CommandSource& source);

/// parity: `Commands.performCommand`.
class CommandDispatcher {
public:
    static constexpr std::string_view kUnknownCommand = "Unknown command";
    static constexpr std::string_view kExpectedSeparator =
        "Expected whitespace to end one argument, but found trailing data";

    static constexpr std::int32_t kContextCharacters = 10;

    explicit CommandDispatcher(const CommandEnvironment& environment);

    [[nodiscard]] const CommandEnvironment& environment() const noexcept { return *environment_; }

    [[nodiscard]] const CommandFeedback& feedback() const noexcept { return feedback_; }

    [[nodiscard]] CommandNode& rootNode() noexcept { return root_; }

    [[nodiscard]] const CommandNode& rootNode() const noexcept { return root_; }

    void rebuild();

    CommandNode* registerCommand(std::shared_ptr<const Command> command);

    CommandNode& registerNode(std::string name, const std::function<void(const CommandNodeBuilder&)>& build);

    void unregisterNode(const CommandNode* node);

    CommandNode& registerAlias(std::string name, CommandNode& target);

    std::int32_t execute(const CommandSource& source, const std::string& input) const;

    std::int32_t execute(const CommandSource& source, const std::string& input, std::int32_t& successes) const;

    [[nodiscard]] std::optional<CommandSyntaxError> check(const CommandSource& source, const std::string& input) const;

    std::int32_t continueTail(const CommandNode& node, std::int32_t cursor, const std::string& input,
                              const CommandSource& source, std::uint64_t generation) const;

    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }

    static void deferResult();

    /// parity: `SignableCommand.of`.
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> signableArguments(const CommandSource& source,
                                                                                     const std::string& input) const;

    [[nodiscard]] SuggestionResult suggest(const CommandSource& source, const std::string& input) const;

    [[nodiscard]] wire::protocol::play::CommandsPacket treeFor(const CommandSource& source) const;

    [[nodiscard]] std::vector<std::pair<std::string, std::string>> unreachableSuggestions() const;

    [[nodiscard]] std::vector<CommandNode*> usableCommands(const CommandSource& source) const;

    [[nodiscard]] const CommandNode* deepestParsed(const CommandSource& source, const std::string& input) const;

private:
    /// parity: brigadier's `CommandContext` chain.
    struct Link {
        Arguments arguments;
        SourceModifier modifier;
        bool forks = false;
        CommandExecutor executor;
        const CommandNode* node = nullptr;
        std::int32_t cursor = 0;
    };

    void registerCustom();

    CommandNode* registerBuilt(const Command& command);

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
    std::vector<std::shared_ptr<const Command>> registered_;
};

} // namespace basalt::server::runtime::command
