#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/Campfires.hpp>

#include <algorithm>

namespace basalt::server::runtime::world::campfires {
namespace {

constexpr std::string_view kProgressKey = "CookingTimes";
constexpr std::string_view kTotalKey = "CookingTotalTimes";

/// parity: `cooldownTick`, which takes two off the progress of each slot per tick.
constexpr std::int32_t kCooldownStep = 2;

} // namespace

const wire::common::Identifier& block() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("campfire");
    return instance;
}

std::vector<server::world::loot::ItemStack> tick(State& state, bool lit, const ResultFor& resultFor) {
    if (!lit) {
        for (std::int32_t slot = 0; slot < kSize; ++slot) {
            const auto index = static_cast<std::size_t>(slot);
            if (state.progress[index] > 0) {
                state.progress[index] = std::clamp(state.progress[index] - kCooldownStep, 0, state.total[index]);
            }
        }
        return {};
    }
    std::vector<server::world::loot::ItemStack> done;
    for (std::int32_t slot = 0; slot < kSize; ++slot) {
        const auto index = static_cast<std::size_t>(slot);
        if (!state.items[index].has_value()) {
            continue;
        }
        ++state.progress[index];
        if (state.progress[index] < state.total[index]) {
            continue;
        }
        const server::world::crafting::Recipe* recipe = resultFor ? resultFor(*state.items[index]) : nullptr;
        done.push_back(recipe == nullptr ? *state.items[index] : recipe->result);
        state.items[index].reset();
        state.progress[index] = 0;
        state.total[index] = 0;
    }
    return done;
}

bool placeFood(State& state, const server::world::loot::ItemStack& stack, const ResultFor& resultFor) {
    for (std::int32_t slot = 0; slot < kSize; ++slot) {
        const auto index = static_cast<std::size_t>(slot);
        if (state.items[index].has_value()) {
            continue;
        }
        const server::world::crafting::Recipe* recipe = resultFor ? resultFor(stack) : nullptr;
        if (recipe == nullptr) {
            return false;
        }
        server::world::loot::ItemStack one = stack;
        one.count = 1;
        state.items[index] = std::move(one);
        state.progress[index] = 0;
        state.total[index] = recipe->cookingTime;
        return true;
    }
    return false;
}

bool isLitCampfire(std::int32_t stateId) {
    const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(stateId);
    if (id == nullptr || !isCampfire(id->path())) {
        return false;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto lit = properties.find("lit");
    return lit != properties.end() && lit->second == "true";
}

bool isCampfire(std::string_view path) { return path == "campfire" || path == "soul_campfire"; }

State read(const wire::nbt::NbtCompound& data) {
    State state;
    const blockInventory::Slots slots = blockInventory::read(data, kSize);
    for (std::size_t index = 0; index < slots.size() && index < state.items.size(); ++index) {
        state.items[index] = slots[index];
    }
    const std::span<const std::int32_t> progress = data.getIntArray(kProgressKey);
    for (std::size_t index = 0; index < progress.size() && index < state.progress.size(); ++index) {
        state.progress[index] = progress[index];
    }
    const std::span<const std::int32_t> total = data.getIntArray(kTotalKey);
    for (std::size_t index = 0; index < total.size() && index < state.total.size(); ++index) {
        state.total[index] = total[index];
    }
    return state;
}

void write(const State& state, wire::nbt::NbtCompound& data) {
    blockInventory::write(data, state.items);
    data.putIntArray(kProgressKey, std::vector<std::int32_t>{state.progress.begin(), state.progress.end()});
    data.putIntArray(kTotalKey, std::vector<std::int32_t>{state.total.begin(), state.total.end()});
}

} // namespace basalt::server::runtime::world::campfires
