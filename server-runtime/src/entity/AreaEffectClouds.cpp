#include <basalt/server/runtime/entity/AreaEffectClouds.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::runtime::entity::areaEffectClouds {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

constexpr std::string_view kParticleKey = "custom_particle";
constexpr std::string_view kPotionKey = "potion_contents";

} // namespace

bool isCloud(const wire::common::Identifier& type) {
    return type.namespaceName() == "minecraft" && type.path() == "area_effect_cloud";
}

Cloud read(const ServerEntity& cloud) {
    Cloud settings;
    if (!cloud.sourceNbt.has_value()) {
        return settings;
    }
    const NbtCompound& tag = *cloud.sourceNbt;
    settings.radius = std::clamp(tag.getFloat("Radius", kDefaultRadius), 0.0F, kMaxRadius);
    settings.duration = tag.getInt("Duration", -1);
    settings.waitTime = tag.getInt("WaitTime", 20);
    settings.reapplicationDelay = tag.getInt("ReapplicationDelay", 20);
    settings.durationOnUse = tag.getInt("DurationOnUse");
    settings.radiusOnUse = tag.getFloat("RadiusOnUse");
    settings.radiusPerTick = tag.getFloat("RadiusPerTick");
    settings.potionDurationScale = tag.getFloat("potion_duration_scale", 1.0F);
    if (const NbtCompound* particle = tag.getCompound(kParticleKey); particle != nullptr) {
        settings.particle = std::string{particle->getString("type")};
        settings.particlePower = particle->getFloat("power", 1.0F);
    }
    if (const NbtCompound* potion = tag.getCompound(kPotionKey); potion != nullptr) {
        if (const NbtList* effects = potion->getList("custom_effects"); effects != nullptr && !effects->empty()) {
            if (const NbtCompound* first = (*effects)[0].get<NbtCompound>(); first != nullptr) {
                const auto id = wire::common::Identifier::parse(first->getString("id"));
                if (id.has_value()) {
                    settings.effect = Effect{
                        .id = *id, .amplifier = first->getByte("amplifier"), .duration = first->getInt("duration", 1)};
                }
            }
        }
    }
    if (const NbtTag* owner = tag.find("Owner"); owner != nullptr) {
        settings.owner = *owner;
    }
    return settings;
}

void write(ServerEntity& cloud, const Cloud& settings) {
    if (!cloud.sourceNbt.has_value()) {
        cloud.sourceNbt.emplace();
    }
    NbtCompound& tag = *cloud.sourceNbt;
    tag.putInt("Age", cloud.tickCount);
    tag.putInt("Duration", settings.duration);
    tag.putInt("WaitTime", settings.waitTime);
    tag.putInt("ReapplicationDelay", settings.reapplicationDelay);
    tag.putInt("DurationOnUse", settings.durationOnUse);
    tag.putFloat("RadiusOnUse", settings.radiusOnUse);
    tag.putFloat("RadiusPerTick", settings.radiusPerTick);
    tag.putFloat("Radius", settings.radius);
    if (settings.particle.has_value()) {
        NbtCompound particle;
        particle.putString("type", *settings.particle);
        particle.putFloat("power", settings.particlePower);
        tag.set(kParticleKey, NbtTag::ofCompound(std::move(particle)));
    }
    if (settings.effect.has_value()) {
        NbtCompound effect;
        effect.putString("id", settings.effect->id.toString());
        effect.putByte("amplifier", static_cast<std::int8_t>(settings.effect->amplifier));
        effect.putInt("duration", settings.effect->duration);
        NbtCompound potion;
        potion.set("custom_effects",
                   NbtTag::ofList(NbtList{wire::nbt::NbtType::Compound, {NbtTag::ofCompound(std::move(effect))}}));
        tag.set(kPotionKey, NbtTag::ofCompound(std::move(potion)));
    }
    if (settings.potionDurationScale != 1.0F) {
        tag.putFloat("potion_duration_scale", settings.potionDurationScale);
    }
    if (settings.owner.has_value()) {
        tag.set("Owner", *settings.owner);
    }
}

Cloud dragonBreath(float radius, std::int32_t duration, float radiusPerTick, std::int32_t amplifier) {
    Cloud settings;
    settings.radius = std::clamp(radius, 0.0F, kMaxRadius);
    settings.duration = duration;
    settings.radiusPerTick = radiusPerTick;
    settings.potionDurationScale = 0.25F;
    settings.particle = "minecraft:dragon_breath";
    settings.particlePower = 1.0F;
    settings.effect = Effect{.id = wire::common::Identifier::vanilla("instant_damage"), .amplifier = amplifier};
    return settings;
}

} // namespace basalt::server::runtime::entity::areaEffectClouds
