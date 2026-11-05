#include <basalt/server/runtime/entity/Allays.hpp>
#include <basalt/server/runtime/entity/Armadillos.hpp>
#include <basalt/server/runtime/entity/Axolotls.hpp>
#include <basalt/server/runtime/entity/Breezes.hpp>
#include <basalt/server/runtime/entity/Camels.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/Creakings.hpp>
#include <basalt/server/runtime/entity/Frogs.hpp>
#include <basalt/server/runtime/entity/Goats.hpp>
#include <basalt/server/runtime/entity/Hoglins.hpp>
#include <basalt/server/runtime/entity/Piglins.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Sniffers.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/entity/ai/BrainKit.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/brain/BrainPublish.hpp>
#include <basalt/server/runtime/entity/brain/BrainSensors.hpp>
#include <basalt/server/runtime/entity/brain/BrainState.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>

#include <algorithm>
#include <array>
#include <string>

namespace basalt::server::runtime::entity::brain {
namespace {

constexpr std::int64_t kPlayerScanInterval = 20;
constexpr double kPlayerRange = 16.0;

constexpr std::array<std::string_view, 20> kKeys{
    "attack_target",           "walk_target",  "avoid_target",   "nearest_players",
    "nearest_visible_player",  "home",         "job_site",       "potential_job_site",
    "meeting_point",           "nearest_bed",  "hiding_place",   "look_target",
    "interaction_target",      "breed_target", "hurt_by_entity", "nearest_hostile",
    "golem_detected_recently", "last_slept",   "last_woken",     "last_worked_at_poi"};

constexpr std::array<std::string_view, 12> kActivities{"idle", "fight", "avoid", "admire_item", "celebrate", "ride",
                                                       "swim", "dig",   "roar",  "sniff",       "emerge",    "panic"};

[[nodiscard]] Value entityRef(std::int32_t id) {
    Value value;
    value.kind = Value::Kind::Entity;
    value.whole = id;
    return value;
}

[[nodiscard]] Value place(double x, double y, double z) {
    Value value;
    value.kind = Value::Kind::Position;
    value.at = {x, y, z};
    return value;
}

[[nodiscard]] Value blockPlace(const std::array<std::int32_t, 3>& block) { return place(block[0], block[1], block[2]); }

template <typename Walk>
[[nodiscard]] Value walkOf(const Walk& walk) {
    Value value;
    value.kind = Value::Kind::Walk;
    value.at = walk.at;
    value.whole = walk.entity.value_or(-1);
    value.real = static_cast<double>(walk.speed);
    value.closeEnough = walk.closeEnough;
    return value;
}

void put(BrainState& state, std::string_view key, std::optional<Value> value) {
    if (value.has_value()) {
        state.set(key, std::move(*value));
    } else {
        state.erase(key);
    }
}

[[nodiscard]] std::optional<Value> entityMemory(const std::optional<std::int32_t>& id) {
    return id.has_value() ? std::optional{entityRef(*id)} : std::nullopt;
}

[[nodiscard]] std::optional<Value> entityMemory(const ai::brainKit::Memory<std::int32_t>& memory, std::int64_t now) {
    const std::int32_t* held = memory.get(now);
    return held != nullptr ? std::optional{entityRef(*held)} : std::nullopt;
}

[[nodiscard]] const ai::brainKit::Motion* motionOf(const ServerEntity& mob) {
    if (mob.piglin) {
        return &mob.piglin->motion;
    }
    if (mob.hoglin) {
        return &mob.hoglin->motion;
    }
    if (mob.allay) {
        return nullptr;
    }
    if (mob.axolotl) {
        return &mob.axolotl->motion;
    }
    if (mob.goat) {
        return &mob.goat->motion;
    }
    if (mob.frog) {
        return &mob.frog->motion;
    }
    if (mob.tadpole) {
        return &mob.tadpole->motion;
    }
    if (mob.camel) {
        return &mob.camel->motion;
    }
    if (mob.sniffer) {
        return &mob.sniffer->motion;
    }
    if (mob.armadillo) {
        return &mob.armadillo->motion;
    }
    if (mob.breeze) {
        return &mob.breeze->motion;
    }
    if (mob.creaking) {
        return &mob.creaking->motion;
    }
    return nullptr;
}

[[nodiscard]] std::string_view piglinActivity(piglins::Activity activity) {
    switch (activity) {
    case piglins::Activity::AdmireItem:
        return "admire_item";
    case piglins::Activity::Fight:
        return "fight";
    case piglins::Activity::Avoid:
        return "avoid";
    case piglins::Activity::Celebrate:
        return "celebrate";
    case piglins::Activity::Ride:
        return "ride";
    case piglins::Activity::Idle:
        break;
    }
    return "idle";
}

[[nodiscard]] std::string_view wardenActivity(const ServerEntity& mob) {
    const wardenBrain::WardenState& warden = *mob.warden;
    if (warden.emergeTicks > 0) {
        return "emerge";
    }
    if (warden.digTicks > 0) {
        return "dig";
    }
    if (warden.roarTicks > 0) {
        return "roar";
    }
    if (warden.sniffTicks > 0) {
        return "sniff";
    }
    if (mob.targetId.has_value()) {
        return "fight";
    }
    return warden.disturbanceTicks > 0 ? "investigate" : "idle";
}

[[nodiscard]] std::string_view activityOf(const ServerEntity& mob) {
    if (mob.piglin) {
        return piglinActivity(mob.piglin->activity);
    }
    if (mob.warden) {
        return wardenActivity(mob);
    }
    if (mob.hoglin) {
        return mob.hoglin->activity == hoglins::Activity::Fight   ? "fight"
               : mob.hoglin->activity == hoglins::Activity::Avoid ? "avoid"
                                                                  : "idle";
    }
    if (mob.goat) {
        return mob.goat->activity == goats::Activity::Ram        ? "ram"
               : mob.goat->activity == goats::Activity::LongJump ? "long_jump"
                                                                 : "idle";
    }
    if (mob.frog) {
        switch (mob.frog->activity) {
        case frogs::Activity::Swim:
            return "swim";
        case frogs::Activity::LaySpawn:
            return "lay_spawn";
        case frogs::Activity::Tongue:
            return "tongue";
        case frogs::Activity::LongJump:
            return "long_jump";
        case frogs::Activity::Idle:
            break;
        }
        return "idle";
    }
    if (mob.axolotl) {
        return mob.axolotl->activity == axolotls::Activity::Fight      ? "fight"
               : mob.axolotl->activity == axolotls::Activity::PlayDead ? "play_dead"
                                                                       : "idle";
    }
    if (mob.sniffer) {
        return mob.sniffer->activity == sniffers::Activity::Sniff ? "sniff"
               : mob.sniffer->activity == sniffers::Activity::Dig ? "dig"
                                                                  : "idle";
    }
    if (mob.armadillo) {
        return mob.armadillo->state == armadillos::State::Idle ? "idle" : "panic";
    }
    if (mob.breeze || mob.creaking) {
        return mob.targetId.has_value() ? "fight" : "idle";
    }
    return "idle";
}

void playersAround(ai::MobContext& context, BrainState& state) {
    if ((context.gameTime + context.mob.entityId()) % kPlayerScanInterval != 0) {
        return;
    }
    nearestPlayers(context, state, kPlayerRange);
}

} // namespace

bool hasNativeBrain(const ServerEntity& mob) {
    const wire::common::Identifier& type = mob.type();
    static constexpr std::array<std::string_view, 11> kPaths{"hoglin",    "axolotl",  "goat",    "breeze",
                                                             "armadillo", "sniffer",  "frog",    "tadpole",
                                                             "warden",    "villager", "creaking"};
    return allays::isAllay(type) || piglins::isPiglin(type) || camels::isCamel(type) ||
           copperGolems::isCopperGolem(type) || std::ranges::find(kPaths, type.path()) != kPaths.end();
}

void adopt(const ServerEntity& mob, BrainState& state) {
    if (!hasNativeBrain(mob)) {
        return;
    }
    state.setNativeOwned(true);
    for (const std::string_view key : kKeys) {
        state.registerMemory(key);
    }
    for (const std::string_view name : kActivities) {
        if (!state.hasActivity(name)) {
            state.defineActivity(name, {}, {});
        }
    }
}

void publish(ai::MobContext& context, BrainState& state) {
    const ServerEntity& mob = context.mob;
    const std::int64_t now = context.gameTime;
    if (!state.nativeOwned()) {
        adopt(mob, state);
    }
    state.setActive(activityOf(mob));
    put(state, "attack_target", entityMemory(mob.targetId));
    const ai::brainKit::Motion* motion = motionOf(mob);
    if (mob.allay && mob.allay->walk.has_value()) {
        put(state, "walk_target", walkOf(*mob.allay->walk));
    } else {
        put(state, "walk_target",
            motion != nullptr && motion->walk.has_value() ? std::optional{walkOf(*motion->walk)} : std::nullopt);
    }
    if (mob.piglin) {
        put(state, "avoid_target", entityMemory(mob.piglin->avoidTarget, now));
        put(state, "home", mob.piglin->home.has_value() ? std::optional{blockPlace(*mob.piglin->home)} : std::nullopt);
    } else if (mob.hoglin) {
        put(state, "avoid_target", entityMemory(mob.hoglin->avoidTarget, now));
        put(state, "breed_target", entityMemory(mob.hoglin->breedTarget));
    }
    if (mob.warden && mob.warden->disturbance.has_value()) {
        put(state, "hiding_place", blockPlace(*mob.warden->disturbance));
    }
    if (mob.creaking && mob.creaking->home.has_value()) {
        put(state, "home", blockPlace(*mob.creaking->home));
    }
    playersAround(context, state);
}

void publishVillager(const ServerEntity& self, BrainState& state) {
    if (!state.nativeOwned()) {
        adopt(self, state);
    }
    const villager::VillagerState& own = *self.villager;
    static constexpr std::array<std::string_view, 10> kNames{"core", "idle",  "work", "play",     "rest",
                                                             "meet", "panic", "raid", "pre_raid", "hide"};
    StringSet active;
    for (std::size_t index = 0; index < kNames.size(); ++index) {
        if ((own.active & (1U << index)) != 0) {
            active.emplace(kNames[index]);
            if (!state.hasActivity(kNames[index])) {
                state.defineActivity(kNames[index], {}, {});
            }
        }
    }
    state.replaceActive(std::move(active));
    const villager::Memories& memory = own.memories;
    const auto block = [](const villager::Memory<villager::BlockPos>& held) -> std::optional<Value> {
        return held.present() ? std::optional{place(held.value->x, held.value->y, held.value->z)} : std::nullopt;
    };
    const auto id = [](const villager::Memory<std::int32_t>& held) -> std::optional<Value> {
        return held.present() ? std::optional{entityRef(*held.value)} : std::nullopt;
    };
    put(state, "home", block(memory.home));
    put(state, "job_site", block(memory.jobSite));
    put(state, "potential_job_site", block(memory.potentialJobSite));
    put(state, "meeting_point", block(memory.meetingPoint));
    put(state, "nearest_bed", block(memory.nearestBed));
    put(state, "hiding_place", block(memory.hidingPlace));
    put(state, "interaction_target", id(memory.interactionTarget));
    put(state, "breed_target", id(memory.breedTarget));
    put(state, "hurt_by_entity", id(memory.hurtByEntity));
    put(state, "nearest_hostile", id(memory.nearestHostile));
    put(state, "nearest_visible_player", id(memory.nearestVisiblePlayer));
    if (memory.walkTarget.present()) {
        const villager::WalkTarget& walk = *memory.walkTarget.value;
        Value value;
        value.kind = Value::Kind::Walk;
        value.at = walk.target.entityId != 0 ? std::array<double, 3>{self.x, self.y, self.z} : walk.target.spot;
        value.whole = walk.target.entityId != 0 ? walk.target.entityId : -1;
        value.real = static_cast<double>(walk.speed);
        value.closeEnough = walk.closeEnough;
        state.set("walk_target", std::move(value));
    } else {
        state.erase("walk_target");
    }
    if (memory.nearestPlayers.present()) {
        Value list;
        list.kind = Value::Kind::List;
        for (const std::int32_t each : *memory.nearestPlayers.value) {
            list.items.push_back(entityRef(each));
        }
        put(state, "nearest_players", list.items.empty() ? std::nullopt : std::optional{std::move(list)});
    } else {
        state.erase("nearest_players");
    }
    if (memory.golemDetectedRecently.present()) {
        Value flag;
        flag.kind = Value::Kind::Bool;
        flag.flag = true;
        state.set("golem_detected_recently", flag);
    } else {
        state.erase("golem_detected_recently");
    }
}

} // namespace basalt::server::runtime::entity::brain
