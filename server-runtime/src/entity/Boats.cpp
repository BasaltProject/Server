#include <basalt/server/runtime/entity/Boats.hpp>

namespace basalt::server::runtime::entity::boats {

bool isBoat(const wire::common::Identifier& type) {
    return type.namespaceName() == "minecraft" && (type.path().ends_with("_boat") || type.path().ends_with("_raft"));
}

bool hasChest(const wire::common::Identifier& type) {
    return isBoat(type) && (type.path().ends_with("_chest_boat") || type.path().ends_with("_chest_raft"));
}

std::int32_t maxPassengers(const wire::common::Identifier& type) { return hasChest(type) ? 1 : 2; }

} // namespace basalt::server::runtime::entity::boats
