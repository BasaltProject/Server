#include <basalt/server/runtime/world/BreedingCooldowns.hpp>

#include <mutex>
#include <unordered_map>

namespace basalt::server::runtime::world::breedingCooldowns {
namespace {

std::mutex gLock;
std::unordered_map<std::string, std::int64_t> gEnds;

[[nodiscard]] std::string keyOf(const std::string& player, const wire::common::Identifier& type) {
    return player + "/" + type.toString();
}

} // namespace

bool active(const std::string& player, const wire::common::Identifier& type, const std::int64_t now) {
    const std::scoped_lock guard{gLock};
    const auto found = gEnds.find(keyOf(player, type));
    return found != gEnds.end() && found->second > now;
}

void record(const std::string& player, const wire::common::Identifier& type, const std::int64_t now,
            const std::int32_t seconds) {
    if (seconds <= 0) {
        return;
    }
    const std::scoped_lock guard{gLock};
    gEnds[keyOf(player, type)] = now + (static_cast<std::int64_t>(seconds) * 20);
}

} // namespace basalt::server::runtime::world::breedingCooldowns
