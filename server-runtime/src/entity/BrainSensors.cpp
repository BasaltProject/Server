#include <basalt/server/runtime/entity/ai/BrainKit.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/brain/BrainSensors.hpp>
#include <basalt/server/runtime/entity/brain/BrainState.hpp>

#include <algorithm>

namespace basalt::server::runtime::entity::brain {
namespace {

[[nodiscard]] Value entityRef(std::int32_t id) {
    Value value;
    value.kind = Value::Kind::Entity;
    value.whole = id;
    return value;
}

[[nodiscard]] Value listOf(const std::vector<ai::brainKit::Seen>& seen, bool playersOnly) {
    std::vector<const ai::brainKit::Seen*> picked;
    for (const ai::brainKit::Seen& one : seen) {
        if (!playersOnly || one.player != nullptr) {
            picked.push_back(&one);
        }
    }
    std::ranges::stable_sort(picked,
                             [](const auto* a, const auto* b) { return a->distanceSquared < b->distanceSquared; });
    Value list;
    list.kind = Value::Kind::List;
    for (const auto* one : picked) {
        list.items.push_back(entityRef(one->id));
    }
    return list;
}

void store(BrainState& state, std::string_view key, Value value) {
    if (value.kind == Value::Kind::List && value.items.empty()) {
        state.erase(key);
    } else {
        state.set(key, std::move(value));
    }
}

void livingEntities(ai::MobContext& mob, BrainState& state, double range) {
    const std::vector<ai::brainKit::Seen> all = ai::brainKit::nearby(mob, range, false);
    store(state, "mobs", listOf(all, false));
    store(state, "visible_mobs", listOf(ai::brainKit::nearby(mob, range, true), false));
}

void players(ai::MobContext& mob, BrainState& state, double range) {
    const std::vector<ai::brainKit::Seen> all = ai::brainKit::nearby(mob, range, false);
    store(state, "nearest_players", listOf(all, true));
    Value visible = listOf(ai::brainKit::nearby(mob, range, true), true);
    if (visible.items.empty()) {
        state.erase("nearest_visible_player");
    } else {
        state.set("nearest_visible_player", visible.items.front());
    }
}

} // namespace

void nearestPlayers(ai::MobContext& mob, BrainState& state, double range) { players(mob, state, range); }

void runSensors(ai::MobContext& mob, BrainState& state) {
    for (SensorSpec& sensor : state.sensors()) {
        if (!sensor.started) {
            sensor.started = true;
            // parity: `Sensor.randomlyDelayStart`.
            sensor.nextScan = mob.gameTime + std::max(mob.random.nextInt(sensor.interval), 1) - 1;
        }
        if (mob.gameTime < sensor.nextScan) {
            continue;
        }
        sensor.nextScan = mob.gameTime + sensor.interval;
        if (sensor.name == "nearest_living_entities") {
            livingEntities(mob, state, sensor.range);
        } else if (sensor.name == "nearest_players") {
            players(mob, state, sensor.range);
        }
    }
}

} // namespace basalt::server::runtime::entity::brain
