#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/player/PlayerFreezing.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <string_view>

namespace basalt::server::runtime::player {
namespace {

constexpr std::int32_t kTicksRequiredToFreeze = 140;
constexpr std::int32_t kDecayPerTick = 2;
constexpr std::int32_t kDamageInterval = 40;
constexpr float kFrostSlowdownPerFull = 0.05F;
constexpr double kHalfWidth = 0.3;
constexpr double kHeight = 1.8;
constexpr double kSneakingHeight = 1.5;
constexpr double kSlack = 1.0e-5;

constexpr std::array<std::string_view, 5> kFreezeImmuneWearables{
    "minecraft:leather_boots", "minecraft:leather_leggings", "minecraft:leather_chestplate", "minecraft:leather_helmet",
    "minecraft:leather_horse_armor"};

[[nodiscard]] bool wearsFreezeImmune(const PlayerSession& session) {
    for (std::int32_t slot = 0; slot < PlayerInventory::kArmourSlots; ++slot) {
        const auto& worn = session.inventory.at(PlayerInventory::kArmourStart + slot);
        if (worn.has_value() && std::ranges::find(kFreezeImmuneWearables, worn->id) != kFreezeImmuneWearables.end()) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool touchesPowderSnow(const PlayerSession& session, const entity::entityCollision::BlockAt& blockAt) {
    const double height = session.sneaking ? kSneakingHeight : kHeight;
    namespace mth = wire::common::mth;
    for (std::int32_t x = mth::floor(session.x - kHalfWidth + kSlack); x <= mth::floor(session.x + kHalfWidth - kSlack);
         ++x) {
        for (std::int32_t y = mth::floor(session.y + kSlack); y <= mth::floor(session.y + height - kSlack); ++y) {
            for (std::int32_t z = mth::floor(session.z - kHalfWidth + kSlack);
                 z <= mth::floor(session.z + kHalfWidth - kSlack); ++z) {
                const std::optional<std::int32_t> state = blockAt(x, y, z);
                const wire::common::Identifier* block =
                    state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
                if (block != nullptr && block->path() == "powder_snow") {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

float percentFrozen(const PlayerSession& session) {
    return static_cast<float>(std::min(session.ticksFrozen, kTicksRequiredToFreeze)) /
           static_cast<float>(kTicksRequiredToFreeze);
}

void tickFreezing(PlayerSession& session, const entity::entityCollision::BlockAt& blockAt, const bool freezeDamage,
                  const std::function<void(float)>& hurt) {
    ++session.aliveTicks;
    const bool canFreeze = session.gamemode != wire::protocol::play::Gamemode::Spectator && !wearsFreezeImmune(session);
    session.inPowderSnow = touchesPowderSnow(session, blockAt);
    // parity: `Paper.FreezeLock` stops the count from moving either way, not the damage or the slowdown.
    if (!session.extras.freezeLocked) {
        if (session.inPowderSnow && canFreeze) {
            session.ticksFrozen = std::min(kTicksRequiredToFreeze, session.ticksFrozen + 1);
        } else {
            session.ticksFrozen = std::max(0, session.ticksFrozen - kDecayPerTick);
        }
    }
    session.frostSlowdown = session.ticksFrozen > 0 ? kFrostSlowdownPerFull * percentFrozen(session) : 0.0F;
    if (session.aliveTicks % kDamageInterval == 0 && session.ticksFrozen >= kTicksRequiredToFreeze && canFreeze &&
        freezeDamage && hurt) {
        hurt(1.0F);
    }
}

} // namespace basalt::server::runtime::player
