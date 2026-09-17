#include <basalt/server/runtime/entity/EnderDragon.hpp>

namespace basalt::server::runtime::entity::enderDragon {

bool isDragon(const wire::common::Identifier& type) {
    return type.namespaceName() == "minecraft" && type.path() == "ender_dragon";
}

std::int32_t idSpan(const wire::common::Identifier& type) { return isDragon(type) ? 1 + kPartCount : 1; }

} // namespace basalt::server::runtime::entity::enderDragon
