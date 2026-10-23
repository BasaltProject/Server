package org.basaltmc.bukkit.entity.ops;

import io.papermc.paper.entity.TeleportFlag;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.BasaltServer;
import org.basaltmc.bukkit.Blocks;
import org.basaltmc.bukkit.Text;
import org.basaltmc.bukkit.WorldNatives;
import org.basaltmc.bukkit.entity.Cored;
import org.basaltmc.bukkit.entity.EntityCore;
import org.bukkit.Chunk;
import org.bukkit.Location;
import org.bukkit.Server;
import org.bukkit.World;
import org.bukkit.block.BlockFace;
import org.bukkit.entity.Entity;
import org.bukkit.entity.EntityType;
import org.bukkit.entity.Pose;
import org.bukkit.event.player.PlayerTeleportEvent;
import org.bukkit.metadata.MetadataValue;
import org.bukkit.plugin.Plugin;
import org.bukkit.util.BoundingBox;
import org.bukkit.util.Vector;

public interface EntityOps extends Entity, Cored {
    default int getEntityId() {
        return core().id;
    }

    default UUID getUniqueId() {
        return core().uuid;
    }

    default EntityType getType() {
        return core().type;
    }

    default World getWorld() {
        return core().world;
    }

    default Server getServer() {
        return BasaltServer.instance();
    }

    default Location getLocation() {
        return core().location();
    }

    default Location getLocation(Location into) {
        return core().location(into);
    }

    default double getX() {
        return core().number(EntityCore.kX);
    }

    default double getY() {
        return core().number(EntityCore.kY);
    }

    default double getZ() {
        return core().number(EntityCore.kZ);
    }

    default float getYaw() {
        return (float) core().number(EntityCore.kYaw);
    }

    default float getPitch() {
        return (float) core().number(EntityCore.kPitch);
    }

    default void setRotation(float yaw, float pitch) {
        Location here = core().location();
        Blocks.flushAll();
        WorldNatives.entityMove(core().world.getName(), core().id, here.getX(), here.getY(), here.getZ(), yaw, pitch);
    }

    default Chunk getChunk() {
        Location here = core().location();
        return core().world.getChunkAt(here.getBlockX() >> 4, here.getBlockZ() >> 4);
    }

    default Vector getVelocity() {
        return core().velocity();
    }

    default void setVelocity(Vector velocity) {
        core().velocity(velocity);
    }

    private boolean moveTo(Location location) {
        org.bukkit.event.entity.EntityTeleportEvent event = new org.bukkit.event.entity.EntityTeleportEvent(this,
            getLocation(), location);
        org.bukkit.Bukkit.getPluginManager().callEvent(event);
        return !event.isCancelled() && event.getTo() != null && core().teleport(event.getTo());
    }

    default boolean teleport(Location location) {
        return moveTo(location);
    }

    default boolean teleport(Location location, PlayerTeleportEvent.TeleportCause cause) {
        return moveTo(location);
    }

    default boolean teleport(Location location, PlayerTeleportEvent.TeleportCause cause, TeleportFlag... flags) {
        return moveTo(location);
    }

    default boolean teleport(Entity destination) {
        return moveTo(destination.getLocation());
    }

    default boolean teleport(Entity destination, PlayerTeleportEvent.TeleportCause cause) {
        return moveTo(destination.getLocation());
    }

    default CompletableFuture<Boolean> teleportAsync(Location location, PlayerTeleportEvent.TeleportCause cause,
            TeleportFlag... flags) {
        return CompletableFuture.completedFuture(moveTo(location));
    }

    default void remove() {
        core().remove();
    }

    default boolean isDead() {
        return !core().alive();
    }

    default boolean isValid() {
        return core().alive();
    }

    default boolean isOnGround() {
        return core().number(EntityCore.kGround) != 0.0;
    }

    default boolean isInWorld() {
        return core().alive();
    }

    default int getTicksLived() {
        return core().integer("ticks_lived");
    }

    default void setTicksLived(int value) {
        if (value <= 0) {
            throw new IllegalArgumentException("Age must be at least 1 tick");
        }
        core().set("ticks_lived", value);
    }

    default double getHeight() {
        return core().number(EntityCore.kHeight);
    }

    default double getWidth() {
        return core().number(EntityCore.kWidth);
    }

    default BoundingBox getBoundingBox() {
        return core().box();
    }

    default List<Entity> getNearbyEntities(double x, double y, double z) {
        return core().nearby(x, y, z);
    }

    default BlockFace getFacing() {
        return core().facing();
    }

    default float getFallDistance() {
        return (float) core().real("fall_distance");
    }

    default void setFallDistance(float distance) {
        core().set("fall_distance", (double) distance);
    }

    default int getFireTicks() {
        return core().integer("fire_ticks");
    }

    default void setFireTicks(int ticks) {
        core().set("fire_ticks", Math.max(0, ticks));
    }

    default int getMaxFireTicks() {
        return 20;
    }

    default int getFreezeTicks() {
        return core().integer("frozen_ticks");
    }

    default void setFreezeTicks(int ticks) {
        core().set("frozen_ticks", Math.max(0, ticks));
    }

    default int getMaxFreezeTicks() {
        return 140;
    }

    default boolean isFrozen() {
        return getFreezeTicks() >= getMaxFreezeTicks();
    }

    default boolean isSneaking() {
        return false;
    }

    default boolean isGlowing() {
        return core().flag("glowing");
    }

    default void setGlowing(boolean glowing) {
        core().set("glowing", glowing);
    }

    default boolean isInvisible() {
        return core().flag("invisible");
    }

    default void setInvisible(boolean invisible) {
        core().set("invisible", invisible);
    }

    default boolean isMarkedInvulnerable() {
        return core().flag("invulnerable");
    }

    default void setInvulnerable(boolean invulnerable) {
        core().set("invulnerable", invulnerable);
    }

    default boolean isSilent() {
        return core().flag("silent");
    }

    default void setSilent(boolean silent) {
        core().set("silent", silent);
    }

    default boolean hasGravity() {
        return !core().flag("no_gravity");
    }

    default void setGravity(boolean gravity) {
        core().set("no_gravity", !gravity);
    }

    default boolean isPersistent() {
        return core().flag("persistent");
    }

    default void setPersistent(boolean persistent) {
        core().set("persistent", persistent);
    }

    default boolean isCustomNameVisible() {
        return core().flag("custom_name_visible");
    }

    default void setCustomNameVisible(boolean visible) {
        core().set("custom_name_visible", visible);
    }

    default String getCustomName() {
        return core().text("custom_name");
    }

    default void setCustomName(String name) {
        core().set("custom_name", name == null ? null : Text.plain(
            net.kyori.adventure.text.serializer.legacy.LegacyComponentSerializer.legacySection().deserialize(name)));
    }

    default Component customName() {
        String name = core().text("custom_name");
        return name == null ? null : Component.text(name);
    }

    default void customName(Component name) {
        core().set("custom_name", name == null ? null : Text.plain(name));
    }

    default String getName() {
        String name = core().text("custom_name");
        return name == null ? core().type.getKey().getKey() : name;
    }

    default Component name() {
        return Component.text(getName());
    }

    default String getScoreboardEntryName() {
        return core().uuid.toString();
    }

    default List<Entity> getPassengers() {
        List<Entity> riders = new ArrayList<>();
        for (String id : core().list("passengers")) {
            Entity rider = core().entity(Integer.parseInt(id));
            if (rider != null) {
                riders.add(rider);
            }
        }
        return riders;
    }

    default Entity getPassenger() {
        List<Entity> riders = getPassengers();
        return riders.isEmpty() ? null : riders.get(0);
    }

    default boolean setPassenger(Entity passenger) {
        eject();
        return addPassenger(passenger);
    }

    default boolean addPassenger(Entity passenger) {
        return passenger instanceof Cored rider && passenger != this && rider.core().test("mount", core().id);
    }

    default boolean removePassenger(Entity passenger) {
        return passenger instanceof Cored rider && passenger.getVehicle() == this && rider.core().test("dismount");
    }

    default boolean isEmpty() {
        return core().list("passengers").length == 0;
    }

    default boolean eject() {
        return core().test("eject");
    }

    default Entity getVehicle() {
        return core().entity(core().integer("vehicle"));
    }

    default boolean isInsideVehicle() {
        return core().integer("vehicle") >= 0;
    }

    default boolean leaveVehicle() {
        return core().test("dismount");
    }

    default Set<String> getScoreboardTags() {
        return new HashSet<>(List.of(core().list("tags")));
    }

    default boolean addScoreboardTag(String tag) {
        List<String> tags = new ArrayList<>(List.of(core().list("tags")));
        if (tags.size() >= 1024 || tags.contains(tag)) {
            return false;
        }
        tags.add(tag);
        return core().set("tags", tags.toArray(new String[0]));
    }

    default boolean removeScoreboardTag(String tag) {
        List<String> tags = new ArrayList<>(List.of(core().list("tags")));
        return tags.remove(tag) && core().set("tags", tags.toArray(new String[0]));
    }

    default void setMetadata(String name, MetadataValue value) {
        core().metadata.set(name, value);
    }

    default List<MetadataValue> getMetadata(String name) {
        return core().metadata.get(name);
    }

    default boolean hasMetadata(String name) {
        return core().metadata.has(name);
    }

    default void removeMetadata(String name, Plugin plugin) {
        core().metadata.remove(name, plugin);
    }

    default void sendMessage(String message) {}

    default void sendMessage(String... messages) {}

    default boolean isOp() {
        return false;
    }

    default boolean hasPermission(String permission) {
        return false;
    }

    default Pose getPose() {
        return core().flag("sleeping") ? Pose.SLEEPING : Pose.STANDING;
    }

    default boolean isTicking() {
        return core().alive();
    }

    default boolean isVisibleByDefault() {
        return true;
    }

    default int getPortalCooldown() {
        return 0;
    }

    default boolean isInWater() {
        return core().world.getBlockAt(getLocation()).getType() == org.bukkit.Material.WATER;
    }

    default boolean isInLava() {
        return core().world.getBlockAt(getLocation()).getType() == org.bukkit.Material.LAVA;
    }

    default boolean fromMobSpawner() {
        return false;
    }

    default org.bukkit.entity.EntitySnapshot createSnapshot() {
        return org.basaltmc.bukkit.entity.BasaltEntitySnapshot.of(core());
    }

    default Entity copy() {
        return copy(getLocation());
    }

    default Entity copy(Location location) {
        org.bukkit.entity.EntitySnapshot snapshot = createSnapshot();
        return snapshot == null ? null : snapshot.createEntity(location);
    }

    default org.bukkit.event.entity.EntityDamageEvent getLastDamageCause() {
        return core().lastDamageCause;
    }

    default void setLastDamageCause(org.bukkit.event.entity.EntityDamageEvent event) {
        core().lastDamageCause = event;
    }

    default boolean isInvulnerableTo(org.bukkit.damage.DamageSource source) {
        String kind = source.getDamageType().getKey().getKey();
        return isMarkedInvulnerable() && !kind.equals("out_of_world") && !kind.equals("generic_kill");
    }
}
