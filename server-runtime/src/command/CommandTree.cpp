#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/command/CommandTree.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace basalt::server::runtime::command {
namespace {

using wire::common::Identifier;
using wire::protocol::play::CommandSuggestion;

[[nodiscard]] std::string lowered(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char letter : text) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(letter))));
    }
    return result;
}

/// parity: `SharedSuggestionProvider.matchesSubStr`, which restarts at every `.`, `_` or `/`.
[[nodiscard]] bool matchesSubString(std::string_view pattern, std::string_view input) {
    constexpr std::array<char, 3> kSplitters{'.', '_', '/'};
    std::size_t index = 0;
    while (!input.substr(std::min(index, input.size())).starts_with(pattern)) {
        const std::size_t splitter = input.find_first_of(std::string_view{kSplitters.data(), kSplitters.size()}, index);
        if (splitter == std::string_view::npos) {
            return false;
        }
        index = splitter + 1;
    }
    return true;
}

} // namespace

SuggestionContext::SuggestionContext(const CommandSource& source, const CommandDispatcher& dispatcher,
                                     std::string remaining, const Arguments& arguments)
    : source_(&source), dispatcher_(&dispatcher), remaining_(std::move(remaining)), arguments_(&arguments) {}

const CommandEnvironment& SuggestionContext::environment() const { return dispatcher_->environment(); }

world::LoadedWorld* SuggestionContext::world() const { return worldIn(*source_, environment()); }

std::vector<CommandSuggestion> SuggestionContext::matching(const std::vector<std::string>& values) const {
    const std::string pattern = lowered(remaining_);
    std::vector<std::string> matched;
    for (const std::string& value : values) {
        if (matchesSubString(pattern, lowered(value))) {
            matched.push_back(value);
        }
    }
    std::ranges::sort(matched);
    std::vector<CommandSuggestion> suggestions;
    suggestions.reserve(matched.size());
    for (std::string& value : matched) {
        suggestions.push_back(CommandSuggestion{.text = std::move(value)});
    }
    return suggestions;
}

std::vector<CommandSuggestion> SuggestionContext::matchingIds(const std::vector<Identifier>& values) const {
    const std::string pattern = lowered(remaining_);
    const bool qualified = remaining_.find(':') != std::string::npos;
    std::vector<std::string> matched;
    for (const Identifier& value : values) {
        const bool hit =
            qualified ? matchesSubString(pattern, value.toString())
                      : matchesSubString(pattern, value.namespaceName()) || matchesSubString(pattern, value.path());
        if (hit) {
            matched.push_back(value.toString());
        }
    }
    std::ranges::sort(matched);
    std::vector<CommandSuggestion> suggestions;
    suggestions.reserve(matched.size());
    for (std::string& value : matched) {
        suggestions.push_back(CommandSuggestion{.text = std::move(value)});
    }
    return suggestions;
}

CommandNode::CommandNode(NodeKind kind, std::string name, std::shared_ptr<const ArgumentType> type)
    : kind_(kind), name_(std::move(name)), type_(std::move(type)) {}

std::vector<CommandNode*> CommandNode::sortedChildren() const {
    std::vector<CommandNode*> ordered;
    ordered.reserve(children_.size());
    for (const std::unique_ptr<CommandNode>& child : children_) {
        if (child->kind() == NodeKind::Literal) {
            ordered.push_back(child.get());
        }
    }
    for (const std::unique_ptr<CommandNode>& child : children_) {
        if (child->kind() != NodeKind::Literal) {
            ordered.push_back(child.get());
        }
    }
    return ordered;
}

CommandNode& CommandNode::addChild(std::unique_ptr<CommandNode> child) {
    children_.push_back(std::move(child));
    return *children_.back();
}

void CommandNode::removeChild(const CommandNode* child) {
    std::erase_if(children_, [child](const std::unique_ptr<CommandNode>& held) { return held.get() == child; });
}

void CommandNode::clearChildren() { children_.clear(); }

std::optional<std::string> CommandNode::suggestionId() const {
    if (suggestionProviderId.has_value()) {
        return suggestionProviderId;
    }
    return type_ == nullptr ? std::nullopt : type_->clientSuggestions();
}

SuggestionProvider CommandNode::provider() const {
    if (suggestions) {
        return suggestions;
    }
    return type_ == nullptr ? SuggestionProvider{} : type_->suggestions();
}

bool CommandNode::answersOnItsOwn() const { return suggestionId().has_value(); }

bool mayUse(const CommandSource& source, const CommandNode& node) {
    return !node.permission.has_value() || source.hasPermission(*node.permission);
}

void CommandNodeBuilder::permission(std::string permission) const { node_->permission = std::move(permission); }

void CommandNodeBuilder::executes(CommandExecutor executor) const { node_->executor = std::move(executor); }

void CommandNodeBuilder::suggests(SuggestionProvider provider) const {
    if (node_->kind() == NodeKind::Argument) {
        node_->suggestions = std::move(provider);
    }
}

void CommandNodeBuilder::suggestsFromServer(SuggestionProvider provider) const {
    if (node_->kind() != NodeKind::Argument) {
        return;
    }
    node_->suggestions = std::move(provider);
    node_->suggestionProviderId = std::string{suggestionProviders::kAskServer};
}

void CommandNodeBuilder::suggestsFromClient(std::string provider) const {
    if (node_->kind() == NodeKind::Argument) {
        node_->suggestionProviderId = std::move(provider);
    }
}

void CommandNodeBuilder::redirectTo(CommandNode& target, bool forks, SourceModifier modifier) const {
    node_->redirect = &target;
    node_->forks = forks;
    node_->modifier = std::move(modifier);
}

CommandNode& CommandNodeBuilder::literal(std::string name,
                                         const std::function<void(const CommandNodeBuilder&)>& build) const {
    auto child = std::make_unique<CommandNode>(NodeKind::Literal, std::move(name));
    child->requiredLevel = node_->requiredLevel;
    child->permission = node_->permission;
    CommandNode& added = node_->addChild(std::move(child));
    build(CommandNodeBuilder{added, *root_});
    return added;
}

CommandNode& CommandNodeBuilder::argument(std::string name, std::shared_ptr<const ArgumentType> type,
                                          const std::function<void(const CommandNodeBuilder&)>& build) const {
    auto child = std::make_unique<CommandNode>(NodeKind::Argument, std::move(name), std::move(type));
    child->requiredLevel = node_->requiredLevel;
    child->permission = node_->permission;
    CommandNode& added = node_->addChild(std::move(child));
    build(CommandNodeBuilder{added, *root_});
    return added;
}

} // namespace basalt::server::runtime::command
