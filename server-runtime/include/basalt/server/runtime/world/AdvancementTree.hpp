#pragma once

#include <basalt/server/world/advancement/Advancement.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <map>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

class RealAdvancements;

/// parity: `AdvancementTree` and `TreeNodePosition`.
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

    [[nodiscard]] wire::common::Identifier rootOf(const wire::common::Identifier& id) const;

    [[nodiscard]] const std::vector<wire::common::Identifier>& order() const noexcept { return order_; }

private:
    void position(const RealAdvancements& advancements, const wire::common::Identifier& root);

    std::map<wire::common::Identifier, Node> nodes_;
    std::vector<wire::common::Identifier> order_;
};

} // namespace basalt::server::runtime::world
