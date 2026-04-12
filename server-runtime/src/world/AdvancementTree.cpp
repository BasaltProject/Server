#include <basalt/server/runtime/world/AdvancementTree.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime::world {
namespace {

using wire::common::Identifier;

/// parity: `TreeNodePosition`, the Walker layout, in an arena so a parent can hold indices.
class Layout {
public:
    Layout(const std::map<Identifier, AdvancementTree::Node>& nodes, const RealAdvancements& advancements)
        : nodes_(&nodes), advancements_(&advancements) {}

    void run(const Identifier& root, std::map<Identifier, AdvancementTree::Node>& out) {
        const std::int32_t top = make(root, -1, -1, 1, 0);
        firstWalk(top);
        const float min = secondWalk(top, 0.0F, 0, at(top).y);
        if (min < 0.0F) {
            thirdWalk(top, -min);
        }
        for (const Position& position : positions_) {
            AdvancementTree::Node& node = out.at(position.node);
            node.x = static_cast<float>(position.x);
            node.y = position.y;
        }
    }

private:
    struct Position {
        Identifier node;
        std::int32_t parent = -1;
        std::int32_t previousSibling = -1;
        std::int32_t childIndex = 0;
        std::vector<std::int32_t> children;
        std::int32_t ancestor = -1;
        std::int32_t thread = -1;
        std::int32_t x = 0;
        float y = -1.0F;
        float mod = 0.0F;
        float change = 0.0F;
        float shift = 0.0F;
    };

    [[nodiscard]] Position& at(std::int32_t index) { return positions_[static_cast<std::size_t>(index)]; }

    [[nodiscard]] bool displayed(const Identifier& id) const {
        const server::world::advancement::Advancement* advancement = advancements_->find(id);
        return advancement != nullptr && advancement->display.has_value();
    }

    std::int32_t make(const Identifier& node, std::int32_t parent, std::int32_t previous, std::int32_t childIndex,
                      std::int32_t depth) {
        const auto self = static_cast<std::int32_t>(positions_.size());
        positions_.push_back(Position{.node = node,
                                      .parent = parent,
                                      .previousSibling = previous,
                                      .childIndex = childIndex,
                                      .children = {},
                                      .ancestor = self,
                                      .thread = -1,
                                      .x = depth,
                                      .y = -1.0F});
        std::int32_t last = -1;
        for (const Identifier& child : nodes_->at(node).children) {
            last = addChild(self, child, last);
        }
        return self;
    }

    std::int32_t addChild(std::int32_t self, const Identifier& node, std::int32_t previous) {
        if (displayed(node)) {
            const auto index = static_cast<std::int32_t>(at(self).children.size()) + 1;
            previous = make(node, self, previous, index, at(self).x + 1);
            at(self).children.push_back(previous);
        } else {
            for (const Identifier& grandchild : nodes_->at(node).children) {
                previous = addChild(self, grandchild, previous);
            }
        }
        return previous;
    }

    void firstWalk(std::int32_t self) {
        if (at(self).children.empty()) {
            at(self).y = at(self).previousSibling >= 0 ? at(at(self).previousSibling).y + 1.0F : 0.0F;
            return;
        }
        std::int32_t defaultAncestor = -1;
        for (const std::int32_t child : std::vector<std::int32_t>{at(self).children}) {
            firstWalk(child);
            defaultAncestor = apportion(child, defaultAncestor < 0 ? child : defaultAncestor);
        }
        executeShifts(self);
        const float midpoint = (at(at(self).children.front()).y + at(at(self).children.back()).y) / 2.0F;
        if (at(self).previousSibling >= 0) {
            at(self).y = at(at(self).previousSibling).y + 1.0F;
            at(self).mod = at(self).y - midpoint;
        } else {
            at(self).y = midpoint;
        }
    }

    float secondWalk(std::int32_t self, float modSum, std::int32_t depth, float min) {
        at(self).y += modSum;
        at(self).x = depth;
        min = std::min(min, at(self).y);
        for (const std::int32_t child : std::vector<std::int32_t>{at(self).children}) {
            min = secondWalk(child, modSum + at(self).mod, depth + 1, min);
        }
        return min;
    }

    void thirdWalk(std::int32_t self, float offset) {
        at(self).y += offset;
        for (const std::int32_t child : std::vector<std::int32_t>{at(self).children}) {
            thirdWalk(child, offset);
        }
    }

    void executeShifts(std::int32_t self) {
        float shift = 0.0F;
        float change = 0.0F;
        for (auto i = static_cast<std::int32_t>(at(self).children.size()) - 1; i >= 0; --i) {
            Position& child = at(at(self).children[static_cast<std::size_t>(i)]);
            child.y += shift;
            child.mod += shift;
            change += child.change;
            shift += child.shift + change;
        }
    }

    [[nodiscard]] std::int32_t previousOrThread(std::int32_t self) {
        if (at(self).thread >= 0) {
            return at(self).thread;
        }
        return at(self).children.empty() ? -1 : at(self).children.front();
    }

    [[nodiscard]] std::int32_t nextOrThread(std::int32_t self) {
        if (at(self).thread >= 0) {
            return at(self).thread;
        }
        return at(self).children.empty() ? -1 : at(self).children.back();
    }

    std::int32_t apportion(std::int32_t self, std::int32_t defaultAncestor) {
        if (at(self).previousSibling < 0) {
            return defaultAncestor;
        }
        std::int32_t vir = self;
        std::int32_t vor = self;
        std::int32_t vil = at(self).previousSibling;
        std::int32_t vol = at(at(self).parent).children.front();
        float sir = at(self).mod;
        float sor = at(self).mod;
        float sil = at(vil).mod;
        float sol = at(vol).mod;
        while (nextOrThread(vil) >= 0 && previousOrThread(vir) >= 0) {
            vil = nextOrThread(vil);
            vir = previousOrThread(vir);
            vol = previousOrThread(vol);
            vor = nextOrThread(vor);
            at(vor).ancestor = self;
            const float shift = at(vil).y + sil - (at(vir).y + sir) + 1.0F;
            if (shift > 0.0F) {
                moveSubtree(ancestorOf(vil, self, defaultAncestor), self, shift);
                sir += shift;
                sor += shift;
            }
            sil += at(vil).mod;
            sir += at(vir).mod;
            sol += at(vol).mod;
            sor += at(vor).mod;
        }
        if (nextOrThread(vil) >= 0 && nextOrThread(vor) < 0) {
            at(vor).thread = nextOrThread(vil);
            at(vor).mod += sil - sor;
        } else {
            if (previousOrThread(vir) >= 0 && previousOrThread(vol) < 0) {
                at(vol).thread = previousOrThread(vir);
                at(vol).mod += sir - sol;
            }
            defaultAncestor = self;
        }
        return defaultAncestor;
    }

    void moveSubtree(std::int32_t self, std::int32_t right, float shift) {
        const auto subtrees = static_cast<float>(at(right).childIndex - at(self).childIndex);
        if (subtrees != 0.0F) {
            at(right).change -= shift / subtrees;
            at(self).change += shift / subtrees;
        }
        at(right).shift += shift;
        at(right).y += shift;
        at(right).mod += shift;
    }

    [[nodiscard]] std::int32_t ancestorOf(std::int32_t self, std::int32_t other, std::int32_t defaultAncestor) {
        const std::int32_t ancestor = at(self).ancestor;
        const std::vector<std::int32_t>& siblings = at(at(other).parent).children;
        return ancestor >= 0 && std::ranges::contains(siblings, ancestor) ? ancestor : defaultAncestor;
    }

    const std::map<Identifier, AdvancementTree::Node>* nodes_;
    const RealAdvancements* advancements_;
    std::vector<Position> positions_;
};

} // namespace

AdvancementTree::AdvancementTree(const RealAdvancements& advancements, const wire::common::Logger& logger) {
    // parity: `AdvancementTree.addAll`: keep inserting whatever has its parent in, until nothing moves.
    std::vector<Identifier> pending = advancements.ids();
    while (!pending.empty()) {
        const std::size_t before = pending.size();
        std::erase_if(pending, [&](const Identifier& id) {
            const server::world::advancement::Advancement* advancement = advancements.find(id);
            const std::optional<Identifier> parent = advancement == nullptr ? std::nullopt : advancement->parent;
            if (parent.has_value() && !nodes_.contains(*parent)) {
                return false;
            }
            nodes_.insert_or_assign(id, Node{.id = id, .parent = parent});
            order_.push_back(id);
            if (parent.has_value()) {
                nodes_.at(*parent).children.push_back(id);
            }
            return true;
        });
        if (pending.size() == before) {
            logger.error([&pending] {
                std::string named;
                for (const Identifier& id : pending) {
                    named += (named.empty() ? "" : ", ") + id.toString();
                }
                return "Couldn't load advancements: [" + named + "]";
            });
            break;
        }
    }
    // parity: `repositionNodes`, only the roots that are drawn at all.
    for (const Identifier& id : order_) {
        const server::world::advancement::Advancement* advancement = advancements.find(id);
        if (!nodes_.at(id).parent.has_value() && advancement != nullptr && advancement->display.has_value()) {
            position(advancements, id);
        }
    }
}

void AdvancementTree::position(const RealAdvancements& advancements, const Identifier& root) {
    Layout{nodes_, advancements}.run(root, nodes_);
}

const AdvancementTree::Node* AdvancementTree::get(const Identifier& id) const {
    const auto found = nodes_.find(id);
    return found == nodes_.end() ? nullptr : &found->second;
}

Identifier AdvancementTree::rootOf(const Identifier& id) const {
    Identifier current = id;
    while (const Node* node = get(current)) {
        if (!node->parent.has_value()) {
            break;
        }
        current = *node->parent;
    }
    return current;
}

} // namespace basalt::server::runtime::world
