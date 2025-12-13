#pragma once

#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/command/StringReader.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/CommandPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {
class LoadedWorld;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::command {

class CommandDispatcher;
class CommandEnvironment;

/// One parsed argument, whatever its type.
///
/// Type-erased by hand rather than with `std::any`: there is no RTTI in this build, so the type is
/// carried as the address of a per-type static. Asking for the wrong type answers null instead of
/// misreading the bytes, which is the whole point.
class ArgumentValue {
public:
    ArgumentValue() = default;

    template <typename T>
    [[nodiscard]] static ArgumentValue of(T value) {
        ArgumentValue held;
        held.key_ = keyOf<T>();
        held.storage_ = std::make_shared<T>(std::move(value));
        return held;
    }

    template <typename T>
    [[nodiscard]] const T* as() const noexcept {
        return key_ == keyOf<T>() ? static_cast<const T*>(storage_.get()) : nullptr;
    }

    [[nodiscard]] bool has() const noexcept { return storage_ != nullptr; }

private:
    template <typename T>
    [[nodiscard]] static const void* keyOf() noexcept {
        static const char key = 0;
        return &key;
    }

    const void* key_ = nullptr;
    std::shared_ptr<const void> storage_;
};

/// What a line has parsed so far, by argument name.
using Arguments = std::map<std::string, ArgumentValue, std::less<>>;

/// What a command's completions are computed from: who is typing, the server around them, the text
/// of the argument so far, and whatever earlier arguments on the same line already parsed.
///
/// The earlier arguments are the reason this is not just a string: `/advancement grant <who> only
/// <advancement> <criterion>` can only list the criteria of the advancement that was already named.
class SuggestionContext {
public:
    SuggestionContext(const CommandSource& source, const CommandDispatcher& dispatcher, std::string remaining,
                      const Arguments& arguments);

    [[nodiscard]] const CommandSource& source() const noexcept { return *source_; }

    [[nodiscard]] const CommandDispatcher& dispatcher() const noexcept { return *dispatcher_; }

    [[nodiscard]] const CommandEnvironment& environment() const;

    /// The argument's text so far, as typed.
    [[nodiscard]] const std::string& remaining() const noexcept { return remaining_; }

    /// The world the command would act on, or null when none is loaded.
    [[nodiscard]] world::LoadedWorld* world() const;

    template <typename T>
    [[nodiscard]] const T* argument(std::string_view name) const {
        const auto found = arguments_->find(name);
        return found == arguments_->end() ? nullptr : found->second.template as<T>();
    }

    /// parity: `SharedSuggestionProvider.suggest`, which matches at any `.`, `_` or `/` boundary.
    [[nodiscard]] std::vector<wire::protocol::play::CommandSuggestion>
    matching(const std::vector<std::string>& values) const;

    /// parity: `SharedSuggestionProvider.filterResources`: with no `:` typed, the namespace and the
    /// path are matched separately, so `vill` finds `minecraft:village_plains`.
    [[nodiscard]] std::vector<wire::protocol::play::CommandSuggestion>
    matchingIds(const std::vector<wire::common::Identifier>& values) const;

private:
    const CommandSource* source_;
    const CommandDispatcher* dispatcher_;
    std::string remaining_;
    const Arguments* arguments_;
};

/// Where an argument's completions come from. parity: brigadier's `SuggestionProvider`.
using SuggestionProvider =
    std::function<std::vector<wire::protocol::play::CommandSuggestion>(const SuggestionContext& context)>;

/// The completions for one line, and the slice of it they replace.
struct SuggestionResult {
    std::int32_t start = 0;
    std::int32_t length = 0;
    std::vector<wire::protocol::play::CommandSuggestion> suggestions;

    friend bool operator==(const SuggestionResult&, const SuggestionResult&) = default;
};

/// The vanilla suggestion providers the client resolves on its own.
///
/// A node naming one of these costs no round trip: the client has the same registry and answers
/// from it. Anything else is `kAskServer`, which is what a server-side provider becomes.
namespace suggestionProviders {

inline constexpr std::string_view kAskServer = "minecraft:ask_server";
inline constexpr std::string_view kAvailableSounds = "minecraft:available_sounds";
inline constexpr std::string_view kSummonableEntities = "minecraft:summonable_entities";

} // namespace suggestionProviders

/// Wire-level argument parsing: text in, a value out, plus how the client should be told to parse
/// the same thing.
class ArgumentType {
public:
    ArgumentType() = default;
    ArgumentType(const ArgumentType&) = default;
    ArgumentType& operator=(const ArgumentType&) = default;
    ArgumentType(ArgumentType&&) = default;
    ArgumentType& operator=(ArgumentType&&) = default;
    virtual ~ArgumentType() = default;

    [[nodiscard]] virtual Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource& source,
                                                      const CommandEnvironment& environment) const = 0;

    [[nodiscard]] virtual wire::protocol::play::ArgumentParser wire() const = 0;

    /// Completions every use of this type gets, for a type whose values are the same wherever it
    /// appears. A list that belongs to one command goes on the node instead.
    [[nodiscard]] virtual SuggestionProvider suggestions() const { return {}; }

    /// A vanilla provider the client resolves itself, for types whose values it already has.
    [[nodiscard]] virtual std::optional<std::string> clientSuggestions() const { return std::nullopt; }
};

class CommandContext;

/// A command body. Answers vanilla's "result" integer, which is what `/execute store` reads.
using CommandExecutor = std::function<Parsed<std::int32_t>(const CommandContext& context)>;

/// What a redirecting node does to the source before the rest of the line runs.
///
/// parity: brigadier's `RedirectModifier`, which is how `/execute as @e at @s run ...` works: each
/// link hands the next one its own sources, and a forking link runs the tail once per source.
using SourceModifier = std::function<std::vector<std::shared_ptr<const CommandSource>>(const CommandContext& context)>;

enum class NodeKind : std::uint8_t { Root, Literal, Argument };

/// A node of the server's own command tree.
///
/// Same shape as brigadier's: a root with literal and argument children, each optionally executable
/// and optionally redirecting somewhere else in the tree (which is how `/tp` is an alias of
/// `/teleport` rather than a copy of it). Parsing tries literals before arguments, so a literal
/// child always wins over an argument that would also match its text, matching brigadier's own
/// ordering.
///
/// One struct with a kind rather than three classes: without RTTI a caller cannot ask what a base
/// pointer really is, and the three cases differ by two fields.
class CommandNode {
public:
    CommandNode(NodeKind kind, std::string name, std::shared_ptr<const ArgumentType> type = nullptr);

    [[nodiscard]] NodeKind kind() const noexcept { return kind_; }

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    /// The type this argument parses with, or null on a root or a literal.
    [[nodiscard]] const ArgumentType* type() const noexcept { return type_.get(); }

    [[nodiscard]] const std::vector<std::unique_ptr<CommandNode>>& children() const noexcept { return children_; }

    /// Literals before arguments, which is brigadier's own ordering.
    [[nodiscard]] std::vector<CommandNode*> sortedChildren() const;

    CommandNode& addChild(std::unique_ptr<CommandNode> child);

    void removeChild(const CommandNode* child);

    void clearChildren();

    CommandExecutor executor;

    /// The permission this node needs, e.g. `basalt.command.gamemode`.
    ///
    /// Named, not numeric, because `commands.toml` has to be able to say which permission a command
    /// asks for and `groups.toml` has to be able to grant it, and a number cannot be referred to.
    /// Unset means the node is open to anyone who reached it.
    std::optional<std::string> permission;

    /// parity: what vanilla demands for this command. Recorded, not enforced.
    std::int32_t requiredLevel = permissionLevel::kAll;

    CommandNode* redirect = nullptr;

    /// Set on a redirecting node to change the source the tail runs as.
    SourceModifier modifier;

    /// parity: brigadier's `forks`, which makes the tail run once per source instead of failing on
    /// many.
    bool forks = false;

    /// Set by the suggestion builders: a list that belongs to this command.
    SuggestionProvider suggestions;

    /// The provider name the client is given, if any.
    std::optional<std::string> suggestionProviderId;

    /// What the client is told to complete this argument with, whichever way it was named.
    [[nodiscard]] std::optional<std::string> suggestionId() const;

    /// The provider that answers for this argument, its own or its type's.
    [[nodiscard]] SuggestionProvider provider() const;

    /// Whether the client will ask about this argument at all, which needs the tree to name a
    /// provider for it. Leaving that to the parser is not enough: the client only falls back to the
    /// server for a registry it knows about and has not got, and a wrong guess is invisible from
    /// here (the player just sees the syntax and no list).
    [[nodiscard]] bool answersOnItsOwn() const;

private:
    NodeKind kind_;
    std::string name_;
    std::shared_ptr<const ArgumentType> type_;
    std::vector<std::unique_ptr<CommandNode>> children_;
};

/// Whether a source may use a node. A node with no permission of its own is open to everyone.
[[nodiscard]] bool mayUse(const CommandSource& source, const CommandNode& node);

/// Builder for one node's children, executor and permission requirement.
class CommandNodeBuilder {
public:
    /// `root` is the tree's own root, which is where `/execute run` sends the rest of the line.
    CommandNodeBuilder(CommandNode& node, CommandNode& root) : node_(&node), root_(&root) {}

    explicit CommandNodeBuilder(CommandNode& node) : node_(&node), root_(&node) {}

    /// The node being built, for a command that has to redirect back onto its own tree.
    [[nodiscard]] CommandNode& node() const noexcept { return *node_; }

    [[nodiscard]] CommandNode& root() const noexcept { return *root_; }

    /// Overrides the permission this node needs, for a sub-command that should be harder to reach.
    void permission(std::string permission) const;

    void executes(CommandExecutor executor) const;

    /// What this argument completes to when the client asks about it.
    ///
    /// No provider name travels in the tree, which is what vanilla does for an argument whose own
    /// parser names a registry: the client completes from its copy when it has one and asks the
    /// server when it has not. Use `suggestsFromServer` for an argument whose parser carries
    /// nothing to complete from, since the client never asks about one of those on its own.
    void suggests(SuggestionProvider provider) const;

    /// Same, but the tree tells the client to ask. parity: an argument vanilla calls `.suggests` on.
    void suggestsFromServer(SuggestionProvider provider) const;

    /// Hands the completions to the client's own provider by name, so it never asks the server.
    void suggestsFromClient(std::string provider) const;

    void redirectTo(CommandNode& target, bool forks = false, SourceModifier modifier = {}) const;

    CommandNode& literal(std::string name, const std::function<void(const CommandNodeBuilder&)>& build) const;

    CommandNode& argument(std::string name, std::shared_ptr<const ArgumentType> type,
                          const std::function<void(const CommandNodeBuilder&)>& build) const;

private:
    CommandNode* node_;
    CommandNode* root_;
};

} // namespace basalt::server::runtime::command
