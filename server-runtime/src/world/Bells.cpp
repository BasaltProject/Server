#include <basalt/server/runtime/world/Bells.hpp>

namespace basalt::server::runtime::world::bells {
namespace {

constexpr std::string_view kShakingKey = "Basalt.BellShaking";
constexpr std::string_view kTicksKey = "Basalt.BellTicks";
constexpr std::string_view kResonatingKey = "Basalt.BellResonating";
constexpr std::string_view kResonationKey = "Basalt.BellResonationTicks";

} // namespace

State read(const wire::nbt::NbtCompound& data) {
    return State{.ticks = data.getInt(kTicksKey),
                 .shaking = data.getBoolean(kShakingKey),
                 .resonating = data.getBoolean(kResonatingKey),
                 .resonationTicks = data.getInt(kResonationKey)};
}

void write(const State& state, wire::nbt::NbtCompound& data) {
    if (!state.shaking && !state.resonating) {
        static_cast<void>(data.remove(kShakingKey));
        static_cast<void>(data.remove(kTicksKey));
        static_cast<void>(data.remove(kResonatingKey));
        static_cast<void>(data.remove(kResonationKey));
        return;
    }
    data.putBoolean(kShakingKey, state.shaking);
    data.putInt(kTicksKey, state.ticks);
    data.putBoolean(kResonatingKey, state.resonating);
    data.putInt(kResonationKey, state.resonationTicks);
}

bool ringing(const wire::nbt::NbtCompound& data) {
    return data.getBoolean(kShakingKey) || data.getBoolean(kResonatingKey);
}

void ring(wire::nbt::NbtCompound& data) {
    State state = read(data);
    state.resonationTicks = 0;
    state.ticks = 0;
    state.shaking = true;
    write(state, data);
}

void ringAt(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    server::world::ChunkBlockEntity* entity = chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
    if (entity != nullptr && entity->type().path() == "bell") {
        ring(entity->data());
        chunk->markDirty();
    }
}

Step advance(State& state, const bool raidersNearby) {
    Step step;
    if (state.shaking) {
        ++state.ticks;
    }
    if (state.ticks >= kDurationTicks) {
        state.shaking = false;
        state.ticks = 0;
    }
    if (state.ticks >= kTicksBeforeResonation && state.resonationTicks == 0 && raidersNearby) {
        state.resonating = true;
        step.startedResonating = true;
    }
    if (state.resonating) {
        if (state.resonationTicks < kMaxResonationTicks) {
            ++state.resonationTicks;
        } else {
            step.endedResonating = true;
            state.resonating = false;
        }
    }
    return step;
}

} // namespace basalt::server::runtime::world::bells
