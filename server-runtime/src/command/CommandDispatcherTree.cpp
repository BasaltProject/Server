#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <utility>

namespace basalt::server::runtime::command {

using wire::protocol::play::CommandNodeData;
using wire::protocol::play::CommandsPacket;

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
