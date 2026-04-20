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

    [[nodiscard]] bool signable() const noexcept { return signable_; }

    void markSignable() noexcept { signable_ = true; }

private:
    template <typename T>
    [[nodiscard]] static const void* keyOf() noexcept {
        static const char key = 0;
        return &key;
    }

    const void* key_ = nullptr;
    std::shared_ptr<const void> storage_;
    bool signable_ = false;
};

using Arguments = std::map<std::string, ArgumentValue, std::less<>>;

class SuggestionContext {
public:
    SuggestionContext(const CommandSource& source, const CommandDispatcher& dispatcher, std::string remaining,
                      const Arguments& arguments);

    [[nodiscard]] const CommandSource& source() const noexcept { return *source_; }

    [[nodiscard]] const CommandDispatcher& dispatcher() const noexcept { return *dispatcher_; }

    [[nodiscard]] const CommandEnvironment& environment() const;

    [[nodiscard]] const std::string& remaining() const noexcept { return remaining_; }

    [[nodiscard]] world::LoadedWorld* world() const;

    template <typename T>
    [[nodiscard]] const T* argument(std::string_view name) const {
        const auto found = arguments_->find(name);
        return found == arguments_->end() ? nullptr : found->second.template as<T>();
    }

    /// parity: `SharedSuggestionProvider.suggest`.
    [[nodiscard]] std::vector<wire::protocol::play::CommandSuggestion>
    matching(const std::vector<std::string>& values) const;

    /// parity: `SharedSuggestionProvider.filterResources`.
    [[nodiscard]] std::vector<wire::protocol::play::CommandSuggestion>
    matchingIds(const std::vector<wire::common::Identifier>& values) const;

private:
    const CommandSource* source_;
    const CommandDispatcher* dispatcher_;
    std::string remaining_;
    const Arguments* arguments_;
};

/// parity: brigadier's `SuggestionProvider`.
using SuggestionProvider =
    std::function<std::vector<wire::protocol::play::CommandSuggestion>(const SuggestionContext& context)>;

struct SuggestionResult {
    std::int32_t start = 0;
    std::int32_t length = 0;
    std::vector<wire::protocol::play::CommandSuggestion> suggestions;

    friend bool operator==(const SuggestionResult&, const SuggestionResult&) = default;
};

namespace suggestionProviders {

inline constexpr std::string_view kAskServer = "minecraft:ask_server";
inline constexpr std::string_view kAvailableSounds = "minecraft:available_sounds";
inline constexpr std::string_view kSummonableEntities = "minecraft:summonable_entities";

} // namespace suggestionProviders

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

    [[nodiscard]] virtual SuggestionProvider suggestions() const { return {}; }

    [[nodiscard]] virtual std::optional<std::string> clientSuggestions() const { return std::nullopt; }

    /// parity: `SignedArgument`, a type whose text the client signs as a chat message.
    [[nodiscard]] virtual bool signable() const { return false; }
};

class CommandContext;

using CommandExecutor = std::function<Parsed<std::int32_t>(const CommandContext& context)>;

/// parity: brigadier's `RedirectModifier`.
using SourceModifier = std::function<std::vector<std::shared_ptr<const CommandSource>>(const CommandContext& context)>;

enum class NodeKind : std::uint8_t { Root, Literal, Argument };

class CommandNode {
public:
    CommandNode(NodeKind kind, std::string name, std::shared_ptr<const ArgumentType> type = nullptr);

    [[nodiscard]] NodeKind kind() const noexcept { return kind_; }

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    [[nodiscard]] const ArgumentType* type() const noexcept { return type_.get(); }

    [[nodiscard]] const std::vector<std::unique_ptr<CommandNode>>& children() const noexcept { return children_; }

    [[nodiscard]] std::vector<CommandNode*> sortedChildren() const;

    CommandNode& addChild(std::unique_ptr<CommandNode> child);

    void removeChild(const CommandNode* child);

    void clearChildren();

    CommandExecutor executor;

    std::optional<std::string> permission;

    /// parity: what vanilla demands for this command.
    std::int32_t requiredLevel = permissionLevel::kAll;

    CommandNode* redirect = nullptr;

    SourceModifier modifier;

    /// parity: brigadier's `forks`.
    bool forks = false;

    SuggestionProvider suggestions;

    std::optional<std::string> suggestionProviderId;

    [[nodiscard]] std::optional<std::string> suggestionId() const;

    [[nodiscard]] SuggestionProvider provider() const;

    [[nodiscard]] bool answersOnItsOwn() const;

private:
    NodeKind kind_;
    std::string name_;
    std::shared_ptr<const ArgumentType> type_;
    std::vector<std::unique_ptr<CommandNode>> children_;
};

[[nodiscard]] bool mayUse(const CommandSource& source, const CommandNode& node);

class CommandNodeBuilder {
public:
    CommandNodeBuilder(CommandNode& node, CommandNode& root) : node_(&node), root_(&root) {}

    explicit CommandNodeBuilder(CommandNode& node) : node_(&node), root_(&node) {}

    [[nodiscard]] CommandNode& node() const noexcept { return *node_; }

    [[nodiscard]] CommandNode& root() const noexcept { return *root_; }

    void permission(std::string permission) const;

    void executes(CommandExecutor executor) const;

    void suggests(SuggestionProvider provider) const;

    /// parity: an argument vanilla calls `.suggests` on.
    void suggestsFromServer(SuggestionProvider provider) const;

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
