#include <basalt/server/runtime/entity/MobMisc.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <memory>
#include <string_view>

namespace basalt::server::runtime::entity::mobMisc {

State& of(ServerEntity& mob) {
    if (mob.misc == nullptr) {
        mob.misc = std::make_shared<State>();
    }
    return *mob.misc;
}

const State* peek(const ServerEntity& mob) { return mob.misc.get(); }

std::int32_t maxSchoolSize(const ServerEntity& fish) { return fish.type().path() == "salmon" ? 5 : 8; }

bool isSchooling(const ServerEntity& fish) {
    const std::string_view path = fish.type().path();
    return path == "cod" || path == "salmon" || path == "tropical_fish";
}

bool isFollower(const ServerEntity& fish, const Lookup& lookup) {
    const State* held = peek(fish);
    if (held == nullptr || !held->schoolLeader.has_value()) {
        return false;
    }
    const ServerEntity* leader = lookup(*held->schoolLeader);
    return leader != nullptr && !leader->removed && !leader->dying && leader->health > 0.0;
}

bool hasFollowers(const ServerEntity& fish) {
    const State* held = peek(fish);
    return held != nullptr && held->schoolSize > 1;
}

bool canBeFollowed(const ServerEntity& fish) {
    const State* held = peek(fish);
    return held != nullptr && held->schoolSize > 1 && held->schoolSize < maxSchoolSize(fish);
}

void startFollowing(ServerEntity& fish, ServerEntity& leader) {
    of(fish).schoolLeader = leader.entityId();
    ++of(leader).schoolSize;
}

void stopFollowing(ServerEntity& fish, const Lookup& lookup) {
    State& held = of(fish);
    if (held.schoolLeader.has_value()) {
        if (ServerEntity* leader = lookup(*held.schoolLeader); leader != nullptr) {
            --of(*leader).schoolSize;
        }
    }
    held.schoolLeader.reset();
}

} // namespace basalt::server::runtime::entity::mobMisc
