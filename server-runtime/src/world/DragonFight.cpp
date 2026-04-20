#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/DragonFight.hpp>

#include <array>
#include <span>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

constexpr std::array<std::string_view, 5> kStageNames{"start", "preparing_to_summon_pillars", "summoning_pillars",
                                                      "summoning_dragon", "end"};

/// parity: `UUIDUtil.CODEC`, four ints, high word first.
[[nodiscard]] std::vector<std::int32_t> uuidInts(const wire::common::Uuid& uuid) {
    return {static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.high() >> 32U)),
            static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.high())),
            static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.low() >> 32U)),
            static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid.low()))};
}

[[nodiscard]] std::optional<wire::common::Uuid> uuidOf(std::span<const std::int32_t> ints) {
    if (ints.size() != 4) {
        return std::nullopt;
    }
    const auto word = [](std::int32_t high, std::int32_t low) {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(high)) << 32U) |
               static_cast<std::uint64_t>(static_cast<std::uint32_t>(low));
    };
    return wire::common::Uuid{word(ints[0], ints[1]), word(ints[2], ints[3])};
}

} // namespace

std::string_view respawnStageName(RespawnStage stage) noexcept { return kStageNames[static_cast<std::size_t>(stage)]; }

std::optional<RespawnStage> respawnStageByName(std::string_view name) noexcept {
    for (std::size_t index = 0; index < kStageNames.size(); ++index) {
        if (kStageNames[index] == name) {
            return static_cast<RespawnStage>(index);
        }
    }
    return std::nullopt;
}

NbtCompound DragonFight::encode() const {
    NbtCompound tag;
    tag.putBoolean("needs_state_scanning", needsStateScanning);
    tag.putBoolean("dragon_killed", dragonKilled);
    tag.putBoolean("previously_killed", previouslyKilled);
    if (respawnStage.has_value()) {
        tag.putString("respawn_stage", std::string{respawnStageName(*respawnStage)});
    }
    tag.putInt("respawn_time", respawnTime);
    if (dragonUuid.has_value()) {
        tag.putIntArray("dragon_uuid", uuidInts(*dragonUuid));
    }
    if (exitPortalLocation.has_value()) {
        tag.putIntArray("exit_portal_location",
                        {(*exitPortalLocation)[0], (*exitPortalLocation)[1], (*exitPortalLocation)[2]});
    }
    tag.putIntArray("gateways", gateways);
    if (!respawnCrystals.empty()) {
        wire::nbt::NbtList crystals{wire::nbt::NbtType::IntArray};
        for (const wire::common::Uuid& crystal : respawnCrystals) {
            static_cast<void>(crystals.push(NbtTag::ofIntArray(uuidInts(crystal))));
        }
        tag.set("respawn_crystals", NbtTag::ofList(std::move(crystals)));
    }
    return tag;
}

DragonFight DragonFight::decode(const NbtCompound& tag) {
    DragonFight fight;
    fight.needsStateScanning = tag.getBoolean("needs_state_scanning", true);
    fight.dragonKilled = tag.getBoolean("dragon_killed");
    fight.previouslyKilled = tag.getBoolean("previously_killed");
    fight.respawnStage = respawnStageByName(tag.getString("respawn_stage"));
    fight.respawnTime = tag.getInt("respawn_time");
    fight.dragonUuid = uuidOf(tag.getIntArray("dragon_uuid"));
    if (const std::span<const std::int32_t> portal = tag.getIntArray("exit_portal_location"); portal.size() == 3) {
        fight.exitPortalLocation = std::array<std::int32_t, 3>{portal[0], portal[1], portal[2]};
    }
    const std::span<const std::int32_t> gateways = tag.getIntArray("gateways");
    fight.gateways.assign(gateways.begin(), gateways.end());
    if (const wire::nbt::NbtList* crystals = tag.getList("respawn_crystals"); crystals != nullptr) {
        for (const NbtTag& crystal : *crystals) {
            if (const auto* ints = crystal.get<std::vector<std::int32_t>>(); ints != nullptr) {
                if (std::optional<wire::common::Uuid> uuid = uuidOf(*ints); uuid.has_value()) {
                    fight.respawnCrystals.push_back(*uuid);
                }
            }
        }
    }
    return fight;
}

void DragonFight::shuffleGateways(std::int64_t seed) {
    if (!gateways.empty()) {
        return;
    }
    for (std::int32_t index = 0; index < kGatewayCount; ++index) {
        gateways.push_back(index);
    }
    // parity: `Util.shuffle` over `RandomSource.createThreadLocalInstance(seed)`.
    random::RandomSource random = random::RandomSource::legacy(seed);
    for (auto index = static_cast<std::int32_t>(gateways.size()); index > 1; --index) {
        const std::int32_t swapTo = random.nextInt(index);
        std::swap(gateways[static_cast<std::size_t>(index - 1)], gateways[static_cast<std::size_t>(swapTo)]);
    }
}

} // namespace basalt::server::runtime::world
