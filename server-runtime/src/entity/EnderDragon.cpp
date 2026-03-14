#include <basalt/server/runtime/entity/EnderDragon.hpp>

namespace basalt::server::runtime::entity::enderDragon {

bool isDragon(const wire::common::Identifier& type) {
    return type.namespaceName() == "minecraft" && type.path() == "ender_dragon";
}

std::optional<Part> partByEntityId(std::int32_t dragonId, std::int32_t entityId) {
    const std::int32_t offset = entityId - dragonId - 1;
    if (offset < 0 || offset >= kPartCount) {
        return std::nullopt;
    }
    return static_cast<Part>(offset);
}

std::int32_t idSpan(const wire::common::Identifier& type) { return isDragon(type) ? 1 + kPartCount : 1; }

} // namespace basalt::server::runtime::entity::enderDragon
