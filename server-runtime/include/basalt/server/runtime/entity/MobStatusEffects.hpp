#pragma once

#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace mobStatusEffects {

[[nodiscard]] bool canBeAffected(const ServerEntity& mob, const wire::common::Identifier& effect);

[[nodiscard]] bool invertsHealAndHarm(const ServerEntity& mob);

struct Instant {
    float heal = 0.0F;
    float damage = 0.0F;
};

[[nodiscard]] Instant instantOf(const ServerEntity& mob, const wire::common::Identifier& effect,
                                std::int32_t amplifier);

bool add(ServerEntity& mob, const world::ActiveEffect& effect);

[[nodiscard]] bool has(const ServerEntity& mob, const wire::common::Identifier& effect);

bool remove(ServerEntity& mob, const wire::common::Identifier& effect);

void clear(ServerEntity& mob);

struct TickEffect {
    float heal = 0.0F;
    float damage = 0.0F;
    std::string_view cause;
};

[[nodiscard]] std::vector<TickEffect> tick(ServerEntity& mob, std::vector<wire::common::Identifier>& expired);

struct Swirl {
    struct Particle {
        wire::common::Identifier particle;
        std::optional<std::int32_t> argb;
    };

    std::vector<Particle> particles;
    bool allAmbient = true;
};

[[nodiscard]] Swirl swirlOf(const std::map<wire::common::Identifier, world::ActiveEffect>& carried);

[[nodiscard]] wire::nbt::NbtTag encode(const ServerEntity& mob);

void decode(ServerEntity& mob, const wire::nbt::NbtCompound& tag);

} // namespace mobStatusEffects

} // namespace basalt::server::runtime::entity
