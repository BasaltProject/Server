package org.basaltmc.bukkit.entity.ops;

import java.util.Set;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Pose;
import org.bukkit.plugin.Plugin;

public interface EntityExtraOps extends Entity, Cored {
    default boolean hasPermission(String permission) {
        return EntityExtras.permissions(core()).hasPermission(permission);
    }

    default boolean hasPermission(org.bukkit.permissions.Permission permission) {
        return EntityExtras.permissions(core()).hasPermission(permission);
    }

    default boolean isPermissionSet(String name) {
        return EntityExtras.permissions(core()).isPermissionSet(name);
    }

    default boolean isPermissionSet(org.bukkit.permissions.Permission permission) {
        return EntityExtras.permissions(core()).isPermissionSet(permission);
    }

    default org.bukkit.permissions.PermissionAttachment addAttachment(Plugin plugin) {
        return EntityExtras.permissions(core()).addAttachment(plugin);
    }

    default org.bukkit.permissions.PermissionAttachment addAttachment(Plugin plugin, int ticks) {
        return EntityExtras.permissions(core()).addAttachment(plugin, ticks);
    }

    default org.bukkit.permissions.PermissionAttachment addAttachment(Plugin plugin, String name, boolean value) {
        return EntityExtras.permissions(core()).addAttachment(plugin, name, value);
    }

    default org.bukkit.permissions.PermissionAttachment addAttachment(Plugin plugin, String name, boolean value,
            int ticks) {
        return EntityExtras.permissions(core()).addAttachment(plugin, name, value, ticks);
    }

    default void removeAttachment(org.bukkit.permissions.PermissionAttachment attachment) {
        EntityExtras.permissions(core()).removeAttachment(attachment);
    }

    default void recalculatePermissions() {
        EntityExtras.permissions(core()).recalculatePermissions();
    }

    default Set<org.bukkit.permissions.PermissionAttachmentInfo> getEffectivePermissions() {
        return EntityExtras.effective(core());
    }

    default void setOp(boolean value) {
        throw new UnsupportedOperationException("Cannot change the operator status of an entity");
    }

    default void sendMessage(java.util.UUID sender, String message) {}

    default void sendMessage(java.util.UUID sender, String... messages) {}

    default void broadcastHurtAnimation(java.util.Collection<org.bukkit.entity.Player> players) {
        EntityExtras.hurtAnimation(core(), 0.0F, players);
    }

    default float getAirDrag() {
        return EntityExtras.airDrag(this);
    }

    default double getDefaultGravity() {
        return EntityExtras.defaultGravity(this);
    }

    default double getGravity() {
        return hasGravity() ? getDefaultGravity() : 0.0;
    }

    default org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason getEntitySpawnReason() {
        return EntityExtras.spawnReason(core());
    }

    default io.papermc.paper.entity.RemovalReason getRemovalReason() {
        return EntityExtras.removalReason(core());
    }

    default org.bukkit.event.entity.EntityRemoveEvent.Cause getRemoveEventCause() {
        return EntityExtras.removeCause(core());
    }

    default org.bukkit.Sound getSwimSound() {
        return org.bukkit.Sound.ENTITY_GENERIC_SWIM;
    }

    default org.bukkit.Sound getSwimSplashSound() {
        return org.bukkit.Sound.ENTITY_GENERIC_SPLASH;
    }

    default org.bukkit.Sound getSwimHighSpeedSplashSound() {
        return org.bukkit.Sound.ENTITY_GENERIC_SPLASH;
    }

    default Set<org.bukkit.entity.Player> getTrackedBy() {
        return EntityExtras.trackers(this, core());
    }

    default Set<org.bukkit.entity.Player> getTrackedPlayers() {
        return EntityExtras.trackers(this, core());
    }

    default boolean isTrackedBy(org.bukkit.entity.Player player) {
        return EntityExtras.trackers(this, core()).contains(player);
    }

    default net.kyori.adventure.util.TriState getVisualFire() {
        int mode = core().integer("visual_fire");
        return mode < 0 ? net.kyori.adventure.util.TriState.NOT_SET
            : mode > 0 ? net.kyori.adventure.util.TriState.TRUE : net.kyori.adventure.util.TriState.FALSE;
    }

    default void setVisualFire(net.kyori.adventure.util.TriState fire) {
        core().set("visual_fire", fire == net.kyori.adventure.util.TriState.NOT_SET ? -1L
            : fire == net.kyori.adventure.util.TriState.TRUE ? 1L : 0L);
    }

    default void setVisualFire(boolean fire) {
        core().set("visual_fire", fire ? 1L : 0L);
    }

    default boolean isVisualFire() {
        return getVisualFire() == net.kyori.adventure.util.TriState.TRUE;
    }

    default void setPose(Pose pose, boolean fixed) {
        if (fixed) {
            throw org.basaltmc.bukkit.Unsupported.hit("Entity#setPose with a fixed pose: the engine recomputes the pose");
        }
        core().set("pose", (long) pose.ordinal());
    }

    default boolean hasFixedPose() {
        return false;
    }

    default boolean hasNoPhysics() {
        return core().flag("no_physics");
    }

    default void setNoPhysics(boolean noPhysics) {
        core().set("no_physics", noPhysics);
    }

    default boolean isFreezeTickingLocked() {
        return core().flag("freeze_locked");
    }

    default void lockFreezeTicks(boolean locked) {
        core().set("freeze_locked", locked);
    }

    default boolean isInPowderedSnow() {
        return core().world.getBlockAt(getLocation()).getType() == org.bukkit.Material.POWDER_SNOW;
    }

    default void setPortalCooldown(int ticks) {
        core().set("portal_cooldown", (long) Math.max(0, ticks));
    }

    default void setVisibleByDefault(boolean visible) {
        core().set("visible_by_default", visible);
    }

    default void setSneaking(boolean sneaking) {
        core().set("crouching", sneaking);
    }

    default void setRotation(io.papermc.paper.math.Angle yaw, io.papermc.paper.math.Angle pitch) {
        setRotation(yaw.relative() ? getYaw() + yaw.degrees() : yaw.degrees(),
            pitch.relative() ? getPitch() + pitch.degrees() : pitch.degrees());
    }

    default void lookAt(double x, double y, double z, io.papermc.paper.entity.LookAnchor anchor) {
        EntityExtras.lookAt(core(), this, x, y, z, anchor);
    }

    default void playEffect(org.bukkit.EntityEffect effect) {
        EntityExtras.playEffect(core(), this, effect);
    }

    default net.kyori.adventure.sound.Sound.Source soundSource() {
        return switch (getSoundCategory()) {
            case PLAYERS -> net.kyori.adventure.sound.Sound.Source.PLAYER;
            case HOSTILE -> net.kyori.adventure.sound.Sound.Source.HOSTILE;
            default -> net.kyori.adventure.sound.Sound.Source.NEUTRAL;
        };
    }

    default Component teamDisplayName() {
        return EntityExtras.teamDisplayName(this);
    }

    default Entity.Spigot spigot() {
        return EntityExtras.spigot();
    }

    default boolean isVisibleByDefault() {
        return !Boolean.FALSE.equals(core().property("visible_by_default"));
    }

    default int getPortalCooldown() {
        return core().integer("portal_cooldown");
    }

    default Pose getPose() {
        int forced = core().property("pose") instanceof Number n ? n.intValue() : -1;
        Pose[] poses = Pose.values();
        if (forced >= 0 && forced < poses.length) {
            return poses[forced];
        }
        return core().flag("sleeping") ? Pose.SLEEPING : core().flag("gliding") ? Pose.FALL_FLYING
            : core().flag("swimming") ? Pose.SWIMMING : isSneaking() ? Pose.SNEAKING : Pose.STANDING;
    }

    default boolean isSneaking() {
        return core().flag("crouching");
    }
}
