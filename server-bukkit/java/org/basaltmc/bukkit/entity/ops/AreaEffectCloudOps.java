package org.basaltmc.bukkit.entity.ops;

import java.util.List;
import java.util.UUID;
import org.basaltmc.bukkit.entity.Cored;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Bukkit;
import org.bukkit.Color;
import org.bukkit.Particle;
import org.bukkit.entity.AreaEffectCloud;
import org.bukkit.entity.Entity;
import org.bukkit.potion.PotionData;
import org.bukkit.potion.PotionEffect;
import org.bukkit.potion.PotionEffectType;
import org.bukkit.potion.PotionType;
import org.bukkit.projectiles.ProjectileSource;

public interface AreaEffectCloudOps extends AreaEffectCloud, Cored {
    private EntityPotions potion() {
        return new EntityPotions(core(), "potion_contents");
    }

    default int getDuration() {
        return core().nbtInt("Duration", -1);
    }

    default void setDuration(int duration) {
        core().nbtSet("Duration", "int", duration);
    }

    default int getWaitTime() {
        return core().nbtInt("WaitTime", 20);
    }

    default void setWaitTime(int waitTime) {
        core().nbtSet("WaitTime", "int", waitTime);
    }

    default int getReapplicationDelay() {
        return core().nbtInt("ReapplicationDelay", 20);
    }

    default void setReapplicationDelay(int delay) {
        core().nbtSet("ReapplicationDelay", "int", delay);
    }

    default int getDurationOnUse() {
        return core().nbtInt("DurationOnUse");
    }

    default void setDurationOnUse(int duration) {
        core().nbtSet("DurationOnUse", "int", duration);
    }

    default float getRadius() {
        return (float) (core().nbtValue("Radius") instanceof Number radius ? radius.doubleValue() : 3.0);
    }

    default void setRadius(float radius) {
        core().nbtSet("Radius", "float", (double) radius);
    }

    default float getRadiusOnUse() {
        return (float) core().nbtReal("RadiusOnUse");
    }

    default void setRadiusOnUse(float radius) {
        core().nbtSet("RadiusOnUse", "float", (double) radius);
    }

    default float getRadiusPerTick() {
        return (float) core().nbtReal("RadiusPerTick");
    }

    default void setRadiusPerTick(float radius) {
        core().nbtSet("RadiusPerTick", "float", (double) radius);
    }

    default Particle getParticle() {
        String type = EntityNbt.compound(core(), "custom_particle").get("type") instanceof String id ? id
            : "minecraft:entity_effect";
        Particle found = org.bukkit.Registry.PARTICLE_TYPE.get(org.bukkit.NamespacedKey.fromString(type));
        return found == null ? Particle.ENTITY_EFFECT : found;
    }

    default void setParticle(Particle particle) {
        EntityNbt.put(core(), "custom_particle", java.util.Map.of("type", particle.getKey().toString()));
    }

    default <T> void setParticle(Particle particle, T data) {
        if (data != null) {
            throw org.basaltmc.bukkit.Unsupported.hit("AreaEffectCloud#setParticle with particle data");
        }
        setParticle(particle);
    }

    default void setBasePotionData(PotionData data) {
        potion().type(EntityPotions.of(data));
    }

    default PotionData getBasePotionData() {
        return EntityPotions.dataOf(potion().type());
    }

    default void setBasePotionType(PotionType type) {
        potion().type(type);
    }

    default PotionType getBasePotionType() {
        return potion().type();
    }

    default boolean hasCustomEffects() {
        return !potion().effects().isEmpty();
    }

    default List<PotionEffect> getCustomEffects() {
        return potion().effects();
    }

    default boolean addCustomEffect(PotionEffect effect, boolean override) {
        return potion().add(effect, override);
    }

    default boolean removeCustomEffect(PotionEffectType type) {
        return potion().remove(type);
    }

    default boolean hasCustomEffect(PotionEffectType type) {
        return potion().has(type);
    }

    default void clearCustomEffects() {
        potion().clear();
    }

    default Color getColor() {
        Color custom = potion().color();
        if (custom != null) {
            return custom;
        }
        return ((io.papermc.paper.datacomponent.item.PotionContents) org.basaltmc.bukkit.datacomponent.ContentSpecs.POTION
            .read(EntityNbt.compound(core(), "").getOrDefault("potion_contents", Nbt.compound())))
            .computeEffectiveColor();
    }

    default void setColor(Color color) {
        potion().color(color);
    }

    default UUID getOwnerUniqueId() {
        Object stored = EntityNbt.compound(core(), "").get("Owner");
        return stored instanceof int[] ints && ints.length == 4 ? Convert.uuidOf(stored) : null;
    }

    default void setOwnerUniqueId(UUID id) {
        if (id == null) {
            core().nbtRemove("Owner");
        } else {
            core().nbtSet("Owner", "uuid", id.toString());
        }
    }

    default ProjectileSource getSource() {
        UUID owner = getOwnerUniqueId();
        Entity found = owner == null ? null : Bukkit.getEntity(owner);
        return found instanceof ProjectileSource source ? source : null;
    }

    default void setSource(ProjectileSource shooter) {
        setOwnerUniqueId(shooter instanceof Entity entity ? entity.getUniqueId() : null);
    }
}
