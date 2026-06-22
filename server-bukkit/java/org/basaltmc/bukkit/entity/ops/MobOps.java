package org.basaltmc.bukkit.entity.ops;

import org.bukkit.entity.Entity;
import org.bukkit.entity.LivingEntity;
import org.bukkit.entity.Mob;

public interface MobOps extends Mob, org.basaltmc.bukkit.entity.Cored {
    default LivingEntity getTarget() {
        return core().entity(core().integer("target")) instanceof LivingEntity target ? target : null;
    }

    default void setTarget(LivingEntity target) {
        core().set("target", target == null ? -1 : target.getEntityId());
    }

    default boolean isLeftHanded() {
        return core().flag("left_handed");
    }

    default void setLeftHanded(boolean leftHanded) {
        core().set("left_handed", leftHanded);
    }

    default com.destroystokyo.paper.entity.Pathfinder getPathfinder() {
        return new org.basaltmc.bukkit.entity.BasaltPathfinder(core(), this);
    }

    default void lookAt(double x, double y, double z) {
        core().call("look_at", x, y, z);
    }

    default void lookAt(double x, double y, double z, float headRotationSpeed, float maxHeadPitch) {
        core().call("look_at", x, y, z, (double) headRotationSpeed, (double) maxHeadPitch);
    }

    default void lookAt(org.bukkit.Location location) {
        lookAt(location.getX(), location.getY(), location.getZ());
    }

    default void lookAt(org.bukkit.Location location, float headRotationSpeed, float maxHeadPitch) {
        lookAt(location.getX(), location.getY(), location.getZ(), headRotationSpeed, maxHeadPitch);
    }

    default void lookAt(org.bukkit.entity.Entity entity) {
        lookAt(entity.getLocation().getX(), entity.getLocation().getY() + entity.getHeight() * 0.85,
            entity.getLocation().getZ());
    }

    default void lookAt(org.bukkit.entity.Entity entity, float headRotationSpeed, float maxHeadPitch) {
        lookAt(entity.getLocation().getX(), entity.getLocation().getY() + entity.getHeight() * 0.85,
            entity.getLocation().getZ(), headRotationSpeed, maxHeadPitch);
    }

    default boolean shouldDespawnInPeaceful() {
        return "monster".equals(core().text("category"));
    }

    default boolean isInDaylight() {
        org.bukkit.Location here = getLocation();
        return core().world.isDayTime() && core().world.getHighestBlockYAt(here) <= here.getBlockY();
    }

    default int getHeadRotationSpeed() {
        return 10;
    }

    default int getMaxHeadPitch() {
        return 40;
    }

    default boolean isAware() {
        return !core().flag("no_ai");
    }

    default void setAware(boolean aware) {
        core().set("no_ai", !aware);
    }
}
