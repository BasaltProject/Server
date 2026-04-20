#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace mobConversions {

struct Around {
    bool eyeInWater = false;
    bool inPowderSnow = false;
    /// parity: `EnvironmentAttributes.PIGLINS_ZOMBIFY`, off only in the Nether.
    bool piglinsZombify = false;
    std::int32_t cureProgress = 1;
};

struct Conversion {
    wire::common::Identifier into;
    bool keepEquipment = true;
    bool keepCanPickUpLoot = true;
    std::int32_t levelEvent = 0;
    bool nausea = false;
    bool persistent = false;
    std::string_view sound;
    float soundVolume = 1.0F;
    bool quietInPeace = false;
    enum class Finalize : std::uint8_t { None, Frog, CuredVillager, ZombieVillager };
    Finalize finalize = Finalize::None;
};

[[nodiscard]] std::optional<std::uint8_t> shakingIndexOf(std::string_view typePath);

[[nodiscard]] bool isShaking(const ServerEntity& mob);

inline constexpr std::int32_t kTadpoleTicks = 24000;

inline constexpr std::int32_t kCureMinTicks = 3600;
inline constexpr std::int32_t kCureSpread = 2401;

[[nodiscard]] std::optional<Conversion> tick(ServerEntity& mob, const Around& around);

[[nodiscard]] wire::nbt::NbtCompound convertedTag(const wire::nbt::NbtCompound& from,
                                                  const wire::common::Identifier& fromType,
                                                  const Conversion& conversion);

} // namespace mobConversions

} // namespace basalt::server::runtime::entity
