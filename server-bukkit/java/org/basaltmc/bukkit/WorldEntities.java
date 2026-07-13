package org.basaltmc.bukkit;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Locale;
import java.util.UUID;
import java.util.function.Consumer;
import java.util.function.Predicate;
import org.bukkit.Difficulty;
import org.bukkit.GameRule;
import org.bukkit.HeightMap;
import org.bukkit.Location;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.World;
import org.bukkit.WorldType;
import org.bukkit.block.Biome;
import org.bukkit.block.Block;
import org.bukkit.entity.Entity;
import org.bukkit.entity.EntityType;
import org.bukkit.entity.LivingEntity;
import org.bukkit.entity.Player;
import org.bukkit.util.BoundingBox;
public abstract class WorldEntities extends WorldGeo {
    public abstract Entity entity(int entityId);

    @Override
    public List<Entity> getEntities() {
        List<Entity> found = new ArrayList<>();
        for (int entityId : WorldNatives.entityIds(getName(), 0, 0, true)) {
            Entity entity = entity(entityId);
            if (entity != null) {
                found.add(entity);
            }
        }
        for (Player player : getPlayers()) {
            found.add(player);
        }
        return found;
    }

    @Override
    public int getEntityCount() {
        return getEntities().size();
    }

    @Override
    public List<LivingEntity> getLivingEntities() {
        List<LivingEntity> found = new ArrayList<>();
        for (Entity entity : getEntities()) {
            if (entity instanceof LivingEntity living) {
                found.add(living);
            }
        }
        return found;
    }

    @Override
    public <T extends Entity> Collection<T> getEntitiesByClass(Class<T> type) {
        List<T> found = new ArrayList<>();
        for (Entity entity : getEntities()) {
            if (type.isInstance(entity)) {
                found.add(type.cast(entity));
            }
        }
        return found;
    }

    @Override
    @SafeVarargs
    public final <T extends Entity> Collection<T> getEntitiesByClass(Class<T>... types) {
        List<T> found = new ArrayList<>();
        for (Class<T> type : types) {
            found.addAll(getEntitiesByClass(type));
        }
        return found;
    }

    @Override
    public Collection<Entity> getEntitiesByClasses(Class<?>... types) {
        List<Entity> found = new ArrayList<>();
        for (Entity entity : getEntities()) {
            for (Class<?> type : types) {
                if (type.isInstance(entity)) {
                    found.add(entity);
                    break;
                }
            }
        }
        return found;
    }

    @Override
    public List<Player> getPlayers() {
        List<Player> inside = new ArrayList<>();
        for (Player player : BasaltServer.instance().getOnlinePlayers()) {
            if (player.getWorld().getName().equals(getName())) {
                inside.add(player);
            }
        }
        return inside;
    }

    @Override
    public Entity getEntity(UUID uuid) {
        for (Entity entity : getEntities()) {
            if (entity.getUniqueId().equals(uuid)) {
                return entity;
            }
        }
        return null;
    }

    @Override
    public Collection<Entity> getNearbyEntities(Location location, double x, double y, double z,
            Predicate<? super Entity> filter) {
        List<Entity> found = new ArrayList<>();
        for (Entity entity : getEntities()) {
            Location there = entity.getLocation();
            if (Math.abs(there.getX() - location.getX()) <= x && Math.abs(there.getY() - location.getY()) <= y
                && Math.abs(there.getZ() - location.getZ()) <= z && (filter == null || filter.test(entity))) {
                found.add(entity);
            }
        }
        return found;
    }

    @Override
    public Collection<Entity> getNearbyEntities(BoundingBox box, Predicate<? super Entity> filter) {
        List<Entity> found = new ArrayList<>();
        for (Entity entity : getEntities()) {
            if (box.overlaps(entity.getBoundingBox()) && (filter == null || filter.test(entity))) {
                found.add(entity);
            }
        }
        return found;
    }

    @Override
    public Entity spawnEntity(Location location, EntityType type) {
        return spawnEntity(location, type, org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason.CUSTOM);
    }

    @Override
    public <T extends Entity> T createEntity(Location location, Class<T> type) {
        return org.basaltmc.bukkit.entity.EntitySpawner.create((BasaltWorld) this, location, type);
    }

    @Override
    public <T extends Entity> T addEntity(T entity) {
        return org.basaltmc.bukkit.entity.EntitySpawner.add((BasaltWorld) this, entity,
            org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason.CUSTOM);
    }

    @Override
    public Entity spawnEntity(Location location, EntityType type,
            org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason reason) {
        return org.basaltmc.bukkit.entity.EntitySpawner.spawn((BasaltWorld) this, location, type, reason, null);
    }

    @Override
    public Entity spawnEntity(Location location, EntityType type, boolean randomizeData) {
        return spawnEntity(location, type, org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason.CUSTOM);
    }

    @Override
    public <T extends Entity> T spawn(Location location, Class<T> type, Consumer<? super T> function,
            org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason reason) throws IllegalArgumentException {
        return org.basaltmc.bukkit.entity.EntitySpawner.spawn((BasaltWorld) this, location, type, reason, function);
    }

    @Override
    public <T extends Entity> T spawn(Location location, Class<T> type, boolean randomizeData,
            Consumer<? super T> function) throws IllegalArgumentException {
        return spawn(location, type, function, org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason.CUSTOM);
    }

    @Override
    public <T extends LivingEntity> T spawn(Location location, Class<T> type,
            org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason reason, boolean randomizeData,
            Consumer<? super T> function) throws IllegalArgumentException {
        return spawn(location, type, function, reason);
    }

    @Override
    public org.bukkit.entity.LightningStrike strikeLightning(Location location) {
        return (org.bukkit.entity.LightningStrike) org.basaltmc.bukkit.entity.EntitySpawner.spawn((BasaltWorld) this, location,
            EntityType.LIGHTNING_BOLT, org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason.CUSTOM, null);
    }

    @Override
    public org.bukkit.entity.LightningStrike strikeLightningEffect(Location location) {
        return (org.bukkit.entity.LightningStrike) org.basaltmc.bukkit.entity.EntitySpawner.spawn((BasaltWorld) this, location,
            EntityType.LIGHTNING_BOLT, org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason.CUSTOM,
            bolt -> ((org.basaltmc.bukkit.entity.Cored) bolt).core().set("bolt_visual_only", true));
    }

    @Override
    public org.bukkit.entity.FallingBlock spawnFallingBlock(Location location, org.bukkit.block.data.BlockData data)
            throws IllegalArgumentException {
        int state = BasaltBlockData.stateOf(data);
        return (org.bukkit.entity.FallingBlock) org.basaltmc.bukkit.entity.EntitySpawner.spawn((BasaltWorld) this, location,
            EntityType.FALLING_BLOCK, org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason.CUSTOM,
            block -> ((org.basaltmc.bukkit.entity.Cored) block).core().set("object_data", state));
    }

    @Override
    public <T extends org.bukkit.entity.AbstractArrow> T spawnArrow(Location location, org.bukkit.util.Vector direction,
            float speed, float spread, Class<T> clazz) {
        java.util.concurrent.ThreadLocalRandom random = java.util.concurrent.ThreadLocalRandom.current();
        org.bukkit.util.Vector launch = direction.clone().normalize().multiply(speed).add(new org.bukkit.util.Vector(
            random.nextGaussian() * 0.0075 * spread, random.nextGaussian() * 0.0075 * spread,
            random.nextGaussian() * 0.0075 * spread));
        return org.basaltmc.bukkit.entity.EntitySpawner.spawn((BasaltWorld) this, location, clazz,
            org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason.DEFAULT,
            arrow -> ((org.basaltmc.bukkit.entity.Cored) arrow).core().velocity(launch));
    }

    private boolean blast(Entity source, double x, double y, double z, float power, boolean fire, boolean breakBlocks) {
        if (fire) {
            throw Unsupported.hit("World#createExplosion that sets fires");
        }
        Blocks.flushAll();
        return EntityNatives.explode(getName(), x, y, z, power, breakBlocks,
            source == null ? -1 : source.getEntityId());
    }

    @Override
    public boolean createExplosion(Entity source, Location location, float power, boolean fire, boolean breakBlocks,
            boolean excludeSourceFromDamage) {
        return blast(source, location.getX(), location.getY(), location.getZ(), power, fire, breakBlocks);
    }

    @Override
    public boolean createExplosion(Location location, float power, boolean fire, boolean breakBlocks, Entity source) {
        return blast(source, location.getX(), location.getY(), location.getZ(), power, fire, breakBlocks);
    }

    @Override
    public boolean createExplosion(double x, double y, double z, float power, boolean fire, boolean breakBlocks,
            Entity source) {
        return blast(source, x, y, z, power, fire, breakBlocks);
    }

    @Override
    public org.bukkit.entity.Item dropItem(Location location, org.bukkit.inventory.ItemStack item,
            Consumer<? super org.bukkit.entity.Item> function) {
        return org.basaltmc.bukkit.entity.EntitySpawner.drop((BasaltWorld) this, location, item, 0.0, 0.0, 0.0, function);
    }

    @Override
    public org.bukkit.entity.Item dropItemNaturally(Location location, org.bukkit.inventory.ItemStack item,
            Consumer<? super org.bukkit.entity.Item> function) {
        java.util.concurrent.ThreadLocalRandom random = java.util.concurrent.ThreadLocalRandom.current();
        Location at = location.clone().add(random.nextDouble() * 0.5 + 0.25, random.nextDouble() * 0.5 + 0.25,
            random.nextDouble() * 0.5 + 0.25);
        return org.basaltmc.bukkit.entity.EntitySpawner.drop((BasaltWorld) this, at, item, random.nextDouble() * 0.2 - 0.1, 0.2,
            random.nextDouble() * 0.2 - 0.1, function);
    }
}
