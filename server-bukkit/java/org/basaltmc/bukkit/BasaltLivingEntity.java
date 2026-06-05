package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import io.papermc.paper.entity.TeleportFlag;
import net.kyori.adventure.text.Component;
import org.bukkit.Chunk;
import org.bukkit.Location;
import org.bukkit.Server;
import org.bukkit.World;
import org.bukkit.block.BlockFace;
import org.bukkit.entity.Entity;
import org.bukkit.entity.EntityType;
import org.bukkit.event.player.PlayerTeleportEvent;
import org.bukkit.metadata.MetadataValue;
import org.bukkit.plugin.Plugin;
import org.bukkit.util.BoundingBox;
import org.bukkit.util.Vector;
import org.basaltmc.bukkit.gen.LivingEntityBase;

public final class BasaltLivingEntity extends LivingEntityBase {
    private final EntityCore core;

    BasaltLivingEntity(EntityCore core) {
        this.core = core;
    }

    EntityCore core() {
        return core;
    }

    @Override
    public int getEntityId() {
        return core.id;
    }

    @Override
    public UUID getUniqueId() {
        return core.uuid;
    }

    @Override
    public EntityType getType() {
        return core.type;
    }

    @Override
    public World getWorld() {
        return core.world;
    }

    @Override
    public Server getServer() {
        return BasaltServer.instance();
    }

    @Override
    public Location getLocation() {
        return core.location();
    }

    @Override
    public Location getLocation(Location into) {
        return core.location(into);
    }

    @Override
    public double getX() {
        return core.number(EntityCore.kX);
    }

    @Override
    public double getY() {
        return core.number(EntityCore.kY);
    }

    @Override
    public double getZ() {
        return core.number(EntityCore.kZ);
    }

    @Override
    public float getYaw() {
        return (float) core.number(EntityCore.kYaw);
    }

    @Override
    public float getPitch() {
        return (float) core.number(EntityCore.kPitch);
    }

    @Override
    public void setRotation(float yaw, float pitch) {
        Location here = core.location();
        WorldNatives.entityMove(core.world.getName(), core.id, here.getX(), here.getY(), here.getZ(), yaw, pitch);
    }

    @Override
    public Chunk getChunk() {
        Location here = core.location();
        return core.world.getChunkAt(here.getBlockX() >> 4, here.getBlockZ() >> 4);
    }

    @Override
    public Vector getVelocity() {
        return core.velocity();
    }

    @Override
    public void setVelocity(Vector velocity) {
        core.velocity(velocity);
    }

    @Override
    public boolean teleport(Location location) {
        return core.teleport(location);
    }

    @Override
    public boolean teleport(Location location, PlayerTeleportEvent.TeleportCause cause) {
        return core.teleport(location);
    }

    @Override
    public boolean teleport(Location location, PlayerTeleportEvent.TeleportCause cause, TeleportFlag... flags) {
        return core.teleport(location);
    }

    @Override
    public boolean teleport(Entity destination) {
        return core.teleport(destination.getLocation());
    }

    @Override
    public boolean teleport(Entity destination, PlayerTeleportEvent.TeleportCause cause) {
        return core.teleport(destination.getLocation());
    }

    @Override
    public CompletableFuture<Boolean> teleportAsync(Location location, PlayerTeleportEvent.TeleportCause cause,
            TeleportFlag... flags) {
        return CompletableFuture.completedFuture(core.teleport(location));
    }

    @Override
    public void remove() {
        core.remove();
    }

    @Override
    public boolean isDead() {
        return !core.alive();
    }

    @Override
    public boolean isValid() {
        return core.alive();
    }

    @Override
    public boolean isOnGround() {
        return core.number(EntityCore.kGround) != 0.0;
    }

    @Override
    public boolean isInWorld() {
        return core.alive();
    }

    @Override
    public int getTicksLived() {
        return (int) core.number(EntityCore.kAge);
    }

    @Override
    public void setTicksLived(int value) {}

    @Override
    public double getHeight() {
        return core.number(EntityCore.kHeight);
    }

    @Override
    public double getWidth() {
        return core.number(EntityCore.kWidth);
    }

    @Override
    public BoundingBox getBoundingBox() {
        return core.box();
    }

    @Override
    public List<Entity> getNearbyEntities(double x, double y, double z) {
        return core.nearby(x, y, z);
    }

    @Override
    public BlockFace getFacing() {
        return core.facing();
    }

    @Override
    public float getFallDistance() {
        return 0.0f;
    }

    @Override
    public void setFallDistance(float distance) {}

    @Override
    public int getFireTicks() {
        return 0;
    }

    @Override
    public void setFireTicks(int ticks) {}

    @Override
    public int getMaxFireTicks() {
        return 20;
    }

    @Override
    public boolean isSneaking() {
        return false;
    }

    @Override
    public boolean isGlowing() {
        return false;
    }

    @Override
    public boolean isInvisible() {
        return false;
    }

    @Override
    public boolean isInvulnerable() {
        return false;
    }

    @Override
    public boolean isSilent() {
        return false;
    }

    @Override
    public boolean hasGravity() {
        return true;
    }

    @Override
    public boolean isPersistent() {
        return true;
    }

    @Override
    public void setPersistent(boolean persistent) {}

    @Override
    public boolean isCustomNameVisible() {
        return false;
    }

    @Override
    public String getCustomName() {
        return core.customName();
    }

    @Override
    public Component customName() {
        String name = core.customName();
        return name == null ? null : Component.text(name);
    }

    @Override
    public String getName() {
        String name = core.customName();
        return name == null ? core.type.getKey().getKey() : name;
    }

    @Override
    public Component name() {
        return Component.text(getName());
    }

    @Override
    public List<Entity> getPassengers() {
        return new ArrayList<>();
    }

    @Override
    public boolean addPassenger(Entity passenger) {
        return false;
    }

    @Override
    public boolean removePassenger(Entity passenger) {
        return false;
    }

    @Override
    public boolean isEmpty() {
        return true;
    }

    @Override
    public boolean eject() {
        return false;
    }

    @Override
    public Entity getVehicle() {
        return null;
    }

    @Override
    public boolean isInsideVehicle() {
        return false;
    }

    @Override
    public boolean leaveVehicle() {
        return false;
    }

    @Override
    public Set<String> getScoreboardTags() {
        return core.tags;
    }

    @Override
    public boolean addScoreboardTag(String tag) {
        return core.tags.add(tag);
    }

    @Override
    public boolean removeScoreboardTag(String tag) {
        return core.tags.remove(tag);
    }

    @Override
    public void setMetadata(String name, MetadataValue value) {
        core.metadata.set(name, value);
    }

    @Override
    public List<MetadataValue> getMetadata(String name) {
        return core.metadata.get(name);
    }

    @Override
    public boolean hasMetadata(String name) {
        return core.metadata.has(name);
    }

    @Override
    public void removeMetadata(String name, Plugin plugin) {
        core.metadata.remove(name, plugin);
    }

    @Override
    public void sendMessage(String message) {}

    @Override
    public void sendMessage(String... messages) {}

    @Override
    public boolean isOp() {
        return false;
    }

    @Override
    public boolean hasPermission(String permission) {
        return false;
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof Entity entity && entity.getUniqueId().equals(core.uuid);
    }

    @Override
    public int hashCode() {
        return core.uuid.hashCode();
    }

    @Override
    public String toString() {
        return "BasaltLivingEntity{type=" + core.type + ",id=" + core.id + "}";
    }

    @Override
    public double getHealth() {
        return core().number(EntityCore.kHealth);
    }

    @Override
    public void setHealth(double health) {
        WorldNatives.entityHealth(core().world.getName(), core().id, health);
    }

    @Override
    public double getMaxHealth() {
        return 20.0;
    }

    @Override
    public void setMaxHealth(double health) {}

    @Override
    public void resetMaxHealth() {}

    @Override
    public void damage(double amount) {
        setHealth(Math.max(0.0, getHealth() - amount));
    }

    @Override
    public double getEyeHeight() {
        return getHeight() * 0.85;
    }

    @Override
    public double getEyeHeight(boolean ignorePose) {
        return getEyeHeight();
    }

    @Override
    public Location getEyeLocation() {
        Location here = getLocation();
        here.setY(here.getY() + getEyeHeight());
        return here;
    }

    @Override
    public boolean hasAI() {
        return true;
    }

    @Override
    public void setAI(boolean ai) {}
}
