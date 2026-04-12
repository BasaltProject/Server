#include <basalt/server/runtime/entity/Parrots.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace basalt::server::runtime::entity::parrots {
namespace {

using namespace std::string_view_literals;

/// parity: `Parrot.MOB_SOUND_MAP`, by mob, and the imitation each gets.
constexpr std::array kImitations{
    std::pair{"blaze"sv, "blaze"sv},
    std::pair{"bogged"sv, "bogged"sv},
    std::pair{"breeze"sv, "breeze"sv},
    std::pair{"camel_husk"sv, "camel_husk"sv},
    std::pair{"cave_spider"sv, "spider"sv},
    std::pair{"creaking"sv, "creaking"sv},
    std::pair{"creeper"sv, "creeper"sv},
    std::pair{"drowned"sv, "drowned"sv},
    std::pair{"elder_guardian"sv, "elder_guardian"sv},
    std::pair{"ender_dragon"sv, "ender_dragon"sv},
    std::pair{"endermite"sv, "endermite"sv},
    std::pair{"evoker"sv, "evoker"sv},
    std::pair{"ghast"sv, "ghast"sv},
    std::pair{"happy_ghast"sv, ""sv},
    std::pair{"guardian"sv, "guardian"sv},
    std::pair{"hoglin"sv, "hoglin"sv},
    std::pair{"husk"sv, "husk"sv},
    std::pair{"illusioner"sv, "illusioner"sv},
    std::pair{"magma_cube"sv, "magma_cube"sv},
    std::pair{"parched"sv, "parched"sv},
    std::pair{"phantom"sv, "phantom"sv},
    std::pair{"piglin"sv, "piglin"sv},
    std::pair{"piglin_brute"sv, "piglin_brute"sv},
    std::pair{"pillager"sv, "pillager"sv},
    std::pair{"ravager"sv, "ravager"sv},
    std::pair{"shulker"sv, "shulker"sv},
    std::pair{"silverfish"sv, "silverfish"sv},
    std::pair{"skeleton"sv, "skeleton"sv},
    std::pair{"slime"sv, "slime"sv},
    std::pair{"spider"sv, "spider"sv},
    std::pair{"stray"sv, "stray"sv},
    std::pair{"vex"sv, "vex"sv},
    std::pair{"vindicator"sv, "vindicator"sv},
    std::pair{"warden"sv, "warden"sv},
    std::pair{"witch"sv, "witch"sv},
    std::pair{"wither"sv, "wither"sv},
    std::pair{"wither_skeleton"sv, "wither_skeleton"sv},
    std::pair{"zoglin"sv, "zoglin"sv},
    std::pair{"zombie"sv, "zombie"sv},
    std::pair{"zombie_horse"sv, "zombie_horse"sv},
    std::pair{"zombie_nautilus"sv, "zombie_nautilus"sv},
    std::pair{"zombie_villager"sv, "zombie_villager"sv},
};

} // namespace

std::optional<std::string_view> imitationOf(std::string_view mobType) {
    const auto found =
        std::ranges::find_if(kImitations, [mobType](const auto& entry) { return entry.first == mobType; });
    return found == kImitations.end() ? std::nullopt : std::optional{found->second};
}

std::string ambientSound(bool peaceful, random::RandomSource& random) {
    if (peaceful || random.nextInt(1000) != 0) {
        return "minecraft:entity.parrot.ambient";
    }
    const std::string_view imitated =
        kImitations[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kImitations.size())))].second;
    return imitated.empty() ? std::string{"minecraft:intentionally_empty"}
                            : "minecraft:entity.parrot.imitate." + std::string{imitated};
}

float pitch(random::RandomSource& random) { return ((random.nextFloat() - random.nextFloat()) * 0.2F) + 1.0F; }

bool canSitOnShoulder(const ServerEntity& parrot) { return parrot.tickCount > kRideCooldown; }

} // namespace basalt::server::runtime::entity::parrots
