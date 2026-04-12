#pragma once

#include <basalt/server/world/advancement/Advancement.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <map>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

class RealAdvancements;

/// The loaded advancements as parents and children, with where the screen draws each one.
///
/// parity: `AdvancementTree` and `TreeNodePosition`. Vanilla keeps children in an identity hash set,
/// so its own sibling order is not reproducible; here siblings keep id order.
class AdvancementTree {
public:
    struct Node {
        wire::common::Identifier id;
        std::optional<wire::common::Identifier> parent;
        std::vector<wire::common::Identifier> children;
        float x = 0.0F;
        float y = 0.0F;
    };

    AdvancementTree() = default;

    explicit AdvancementTree(const RealAdvancements& advancements,
                             const wire::common::Logger& logger = wire::common::Logger{"Advancements"});

    [[nodiscard]] const Node* get(const wire::common::Identifier& id) const;

    /// The root of the tree an advancement is in, itself for a root.
    [[nodiscard]] wire::common::Identifier rootOf(const wire::common::Identifier& id) const;

    [[nodiscard]] const std::vector<wire::common::Identifier>& order() const noexcept { return order_; }

private:
    void position(const RealAdvancements& advancements, const wire::common::Identifier& root);

    std::map<wire::common::Identifier, Node> nodes_;
    /// Insertion order, which is what `AdvancementTree.nodes()` iterates in.
    std::vector<wire::common::Identifier> order_;
};

} // namespace basalt::server::runtime::world
