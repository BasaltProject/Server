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

public final class BasaltWorld extends WorldEntities {
    private static final int kKeepRadius = 2;
    private volatile boolean keepSpawn;
    private static final int kSeed = 0;
    private static final int kMinY = 1;
    private static final int kHeight = 2;
    private static final int kSpawnX = 3;
    private static final int kSpawnY = 4;
    private static final int kSpawnZ = 5;
    private static final int kDayTime = 6;
    private static final int kRaining = 7;
    private static final int kThundering = 8;
    private static final int kGameTime = 9;

    private final String name;
    private final UUID id;
    private final File folder;
    private World.Environment environment;
    private int minHeight = Integer.MIN_VALUE;
    private int maxHeight = Integer.MIN_VALUE;

    BasaltWorld(String name) {
        this.name = name;
        this.id = UUID.nameUUIDFromBytes(("basalt-world:" + name).getBytes(StandardCharsets.UTF_8));
        String[] storage = WorldNatives.worldStrings(name);
        if (storage == null || storage.length < 2 || storage[1].isEmpty()) {
            throw new IllegalStateException("No storage path for world " + name);
        }
        this.folder = new File(storage[1]);
    }

    private long number(int index) {
        long[] values = WorldNatives.worldNumbers(name);
        return values == null ? 0L : values[index];
    }

    private String text(int index) {
        String[] values = WorldNatives.worldStrings(name);
        return values == null ? "" : values[index];
    }

    int stateAt(int x, int y, int z) {
        Blocks.Buf buf = Blocks.of(this);
        return buf != null ? buf.state(x, y, z) : WorldNatives.state(name, x, y, z, true);
    }

    void putState(int x, int y, int z, int state) {
        Blocks.Buf buf = Blocks.of(this);
        if (buf != null) {
            buf.put(x, y, z, state);
        } else {
            WorldNatives.setState(name, x, y, z, state, true);
        }
    }

    @Override
    public org.bukkit.Material getType(int x, int y, int z) {
        int state = stateAt(x, y, z);
        return Registries.materialOfState(state < 0 ? 0 : state);
    }

    @Override
    public org.bukkit.block.data.BlockData getBlockData(int x, int y, int z) {
        int state = stateAt(x, y, z);
        return BasaltBlockData.of(state < 0 ? 0 : state);
    }

    @Override
    public org.bukkit.block.BlockState getBlockState(int x, int y, int z) {
        return getBlockAt(x, y, z).getState();
    }

    @Override
    public void setBlockData(int x, int y, int z, org.bukkit.block.data.BlockData data) {
        putState(x, y, z, BasaltBlockData.stateOf(data));
    }

    @Override
    public void setType(int x, int y, int z, org.bukkit.Material type) {
        int state = Registries.defaultStateOf(type);
        if (state < 0) {
            throw new IllegalArgumentException(type + " is not a block");
        }
        putState(x, y, z, state);
    }

    @Override
    public org.bukkit.ChunkSnapshot getEmptyChunkSnapshot(int x, int z, boolean biome, boolean biomeTempRain) {
        return new BasaltChunkSnapshot(this, x, z, false);
    }

    private volatile int prunePoint = 8192;
    private final java.util.concurrent.ConcurrentHashMap<Integer, Entity> entityObjects =
        new java.util.concurrent.ConcurrentHashMap<>();

    /** One object per entity id, kept while the entity lives, so plugins that hold weak references keep working. */
    public Entity entity(int entityId) {
        Entity known = entityObjects.get(entityId);
        if (known != null) {
            return known;
        }
        Entity made = org.basaltmc.bukkit.entity.EntityCore.make(this, entityId);
        if (made != null) {
            if (entityObjects.size() > prunePoint) {
                entityObjects.values().removeIf(entity -> entity.isDead());
                prunePoint = Math.max(8192, entityObjects.size() * 2);
            }
            entityObjects.put(entityId, made);
        }
        return made;
    }

    public void adopt(int entityId, Entity entity) {
        entityObjects.put(entityId, entity);
    }

    @Override
    public String getName() {
        return name;
    }

    @Override
    public UUID getUID() {
        return id;
    }

    @Override
    public NamespacedKey getKey() {
        return NamespacedKey.fromString("minecraft:" + name.toLowerCase(Locale.ROOT));
    }

    @Override
    public net.kyori.adventure.key.Key key() {
        return getKey();
    }

    @Override
    public Environment getEnvironment() {
        if (environment == null) {
            String dimension = text(0);
            environment = dimension.endsWith("nether") ? Environment.NETHER
                : dimension.endsWith("end") ? Environment.THE_END : Environment.NORMAL;
        }
        return environment;
    }

    @Override
    public long getSeed() {
        return number(kSeed);
    }

    @Override
    public int getMinHeight() {
        if (minHeight == Integer.MIN_VALUE) {
            minHeight = (int) number(kMinY);
        }
        return minHeight;
    }

    @Override
    public int getMaxHeight() {
        if (maxHeight == Integer.MIN_VALUE) {
            maxHeight = (int) (number(kMinY) + number(kHeight));
        }
        return maxHeight;
    }

    @Override
    public int getLogicalHeight() {
        return getMaxHeight();
    }

    @Override
    public File getWorldFolder() {
        // The path remains valid metadata after unload; never fall back to the
        // process directory when the live native world is no longer present.
        return folder;
    }

    @Override
    public WorldType getWorldType() {
        return WorldType.NORMAL;
    }

    @Override
    public boolean canGenerateStructures() {
        return true;
    }

    @Override
    public boolean isHardcore() {
        return RestNatives.worldTunable(name, "hardcore") != 0.0;
    }

    @Override
    public void setHardcore(boolean hardcore) {
        RestNatives.setWorldTunable(name, "hardcore", hardcore ? 1.0 : 0.0);
    }

    @Override
    public boolean isNatural() {
        return getEnvironment() == Environment.NORMAL;
    }

    @Override
    public boolean hasSkyLight() {
        return getEnvironment() != Environment.NETHER;
    }

    @Override
    public boolean hasCeiling() {
        return getEnvironment() == Environment.NETHER;
    }

    @Override
    public double getCoordinateScale() {
        return getEnvironment() == Environment.NETHER ? 8.0 : 1.0;
    }

    @Override
    public Block getBlockAt(int x, int y, int z) {
        return new BasaltBlock(this, x, y, z);
    }

    @Override
    public Block getBlockAt(Location location) {
        return getBlockAt(location.getBlockX(), location.getBlockY(), location.getBlockZ());
    }

    @Override
    public int getHighestBlockYAt(int x, int z) {
        Blocks.flushAll();
        return WorldNatives.highest(name, x, z, "motion_blocking", true);
    }

    @Override
    public int getHighestBlockYAt(int x, int z, HeightMap heightMap) {
        String kind = heightMap == HeightMap.WORLD_SURFACE || heightMap == HeightMap.WORLD_SURFACE_WG ? "surface"
            : heightMap == HeightMap.OCEAN_FLOOR || heightMap == HeightMap.OCEAN_FLOOR_WG ? "ocean_floor"
            : "motion_blocking";
        Blocks.flushAll();
        return WorldNatives.highest(name, x, z, kind, true);
    }

    @Override
    public int getHighestBlockYAt(Location location) {
        return getHighestBlockYAt(location.getBlockX(), location.getBlockZ());
    }

    @Override
    public int getHighestBlockYAt(Location location, HeightMap heightMap) {
        return getHighestBlockYAt(location.getBlockX(), location.getBlockZ(), heightMap);
    }

    @Override
    public Block getHighestBlockAt(int x, int z) {
        return getBlockAt(x, getHighestBlockYAt(x, z), z);
    }

    @Override
    public Block getHighestBlockAt(Location location) {
        return getHighestBlockAt(location.getBlockX(), location.getBlockZ());
    }

    @Override
    public Block getHighestBlockAt(int x, int z, HeightMap heightMap) {
        return getBlockAt(x, getHighestBlockYAt(x, z, heightMap), z);
    }

    @Override
    public Block getHighestBlockAt(Location location, HeightMap heightMap) {
        return getHighestBlockAt(location.getBlockX(), location.getBlockZ(), heightMap);
    }

    @Override
    public Biome getBiome(int x, int y, int z) {
        String id = WorldNatives.biome(name, x, y, z);
        return id == null ? null : Registry.BIOME.get(NamespacedKey.fromString(id));
    }

    @Override
    public Biome getBiome(Location location) {
        return getBiome(location.getBlockX(), location.getBlockY(), location.getBlockZ());
    }

    @Override
    public Biome getComputedBiome(int x, int y, int z) {
        return getBiome(x, y, z);
    }

    @Override
    public void setBiome(int x, int y, int z, Biome biome) {
        WorldNatives.setBiome(name, x, y, z, biome.getKey().toString());
    }

    @Override
    public void setBiome(int x, int z, Biome biome) {
        for (int y = getMinHeight(); y < getMaxHeight(); y += 4) {
            setBiome(x, y, z, biome);
        }
    }

    @Override
    public org.bukkit.Chunk getChunkAt(int x, int z) {
        return getChunkAt(x, z, true);
    }

    @Override
    public org.bukkit.Chunk getChunkAt(int x, int z, boolean generate) {
        if (generate) {
            WorldNatives.loadChunk(name, x, z, true);
        }
        return new BasaltChunk(this, x, z);
    }

    @Override
    public org.bukkit.Chunk getChunkAt(Block block) {
        return getChunkAt(block.getX() >> 4, block.getZ() >> 4);
    }

    @Override
    public void getChunkAtAsync(int x, int z, boolean generate, boolean urgent,
            Consumer<? super org.bukkit.Chunk> callback) {
        callback.accept(getChunkAt(x, z, generate));
    }

    @Override
    public boolean isChunkLoaded(int x, int z) {
        return WorldNatives.chunkLoaded(name, x, z);
    }

    @Override
    public boolean isChunkLoaded(org.bukkit.Chunk chunk) {
        return isChunkLoaded(chunk.getX(), chunk.getZ());
    }

    @Override
    public boolean isChunkGenerated(int x, int z) {
        return isChunkLoaded(x, z);
    }

    @Override
    public boolean isChunkInUse(int x, int z) {
        return isChunkLoaded(x, z);
    }

    @Override
    public org.bukkit.Chunk[] getLoadedChunks() {
        int[] flat = WorldNatives.loadedChunks(name);
        org.bukkit.Chunk[] chunks = new org.bukkit.Chunk[flat.length / 2];
        for (int i = 0; i < chunks.length; i++) {
            chunks[i] = new BasaltChunk(this, flat[i * 2], flat[i * 2 + 1]);
        }
        return chunks;
    }

    @Override
    public void loadChunk(org.bukkit.Chunk chunk) {
        WorldNatives.loadChunk(name, chunk.getX(), chunk.getZ(), true);
    }

    @Override
    public boolean loadChunk(int x, int z, boolean generate) {
        return WorldNatives.loadChunk(name, x, z, generate);
    }

    @Override
    public boolean unloadChunk(int x, int z, boolean save) {
        Blocks.flushAll();
        return WorldNatives.unloadChunk(name, x, z);
    }

    @Override
    public boolean unloadChunkRequest(int x, int z) {
        Blocks.flushAll();
        return WorldNatives.unloadChunk(name, x, z);
    }

    @Override
    public boolean refreshChunk(int x, int z) {
        return isChunkLoaded(x, z);
    }

    @Override
    public boolean getKeepSpawnInMemory() {
        return keepSpawn;
    }

    @Override
    public void setKeepSpawnInMemory(boolean keep) {
        keepSpawn = keep;
        holdSpawn();
    }

    private synchronized void holdSpawn() {
        String token = "bukkit:keep-spawn";
        if (!keepSpawn) {
            WorldNatives.releaseChunks(name, token);
            return;
        }
        int centerX = (int) Math.floor(number(kSpawnX)) >> 4;
        int centerZ = (int) Math.floor(number(kSpawnZ)) >> 4;
        int[] flat = new int[(2 * kKeepRadius + 1) * (2 * kKeepRadius + 1) * 2];
        int at = 0;
        for (int dx = -kKeepRadius; dx <= kKeepRadius; dx++) {
            for (int dz = -kKeepRadius; dz <= kKeepRadius; dz++) {
                flat[at++] = centerX + dx;
                flat[at++] = centerZ + dz;
                WorldNatives.loadChunk(name, centerX + dx, centerZ + dz, true);
            }
        }
        WorldNatives.holdChunks(name, token, flat);
    }

    @Override
    public void save() {
        Blocks.flushAll();
        WorldNatives.saveWorld(name);
    }

    @Override
    public void save(boolean flush) {
        Blocks.flushAll();
        WorldNatives.saveWorld(name);
    }

    @Override
    public boolean isAutoSave() {
        return Boolean.parseBoolean(WorldEnvNatives.setting("autosave"));
    }

    @Override
    public void setAutoSave(boolean value) {
        WorldEnvNatives.setSetting("autosave", Boolean.toString(value));
    }


    @Override
    public Location getSpawnLocation() {
        return new Location(this, number(kSpawnX), number(kSpawnY), number(kSpawnZ));
    }

    @Override
    public boolean setSpawnLocation(Location location) {
        WorldNatives.setSpawn(name, location.getBlockX(), location.getBlockY(), location.getBlockZ());
        if (keepSpawn) {
            holdSpawn();
        }
        return true;
    }

    @Override
    public boolean setSpawnLocation(int x, int y, int z, float angle) {
        WorldNatives.setSpawn(name, x, y, z);
        return true;
    }

    @Override
    public long getTime() {
        return number(kDayTime) % 24000L;
    }

    @Override
    public void setTime(long time) {
        WorldNatives.setTime(name, number(kDayTime) - number(kDayTime) % 24000L + Math.floorMod(time, 24000L));
    }

    @Override
    public long getFullTime() {
        return number(kDayTime);
    }

    @Override
    public void setFullTime(long time) {
        WorldNatives.setTime(name, time);
    }

    @Override
    public boolean isDayTime() {
        long time = getTime();
        return time < 12000L || time >= 23000L;
    }

    @Override
    public long getGameTime() {
        return number(kGameTime);
    }

    @Override
    public boolean hasStorm() {
        return number(kRaining) != 0;
    }

    @Override
    public void setStorm(boolean storm) {
        WorldNatives.setWeather(name, storm, isThundering() && storm);
    }

    @Override
    public boolean isThundering() {
        return number(kThundering) != 0;
    }

    @Override
    public void setThundering(boolean thundering) {
        WorldNatives.setWeather(name, hasStorm() || thundering, thundering);
    }

    @Override
    public boolean getPVP() {
        return Boolean.TRUE.equals(getGameRuleValue(org.bukkit.GameRules.PVP));
    }

    @Override
    public void setPVP(boolean value) {
        setGameRule(org.bukkit.GameRules.PVP, value);
    }

    @Override
    public Difficulty getDifficulty() {
        try {
            return Difficulty.valueOf(text(2).toUpperCase(Locale.ROOT));
        } catch (IllegalArgumentException e) {
            return Difficulty.NORMAL;
        }
    }

    @Override
    public void setDifficulty(Difficulty difficulty) {
        WorldNatives.setDifficulty(name, difficulty.name().toLowerCase(Locale.ROOT));
    }

    @Override
    public int getTickableTileEntityCount() {
        return RestNatives.tickableBlockEntities(name);
    }

    @Override
    public int getSimulationDistance() {
        return (int) RestNatives.worldTunable(name, "simulation_distance");
    }

    @Override
    public String[] getGameRules() {
        String[] ids = WorldNatives.gameRules(name);
        String[] names = new String[ids == null ? 0 : ids.length];
        for (int i = 0; i < names.length; i++) {
            names[i] = BasaltGameRule.legacyName(ids[i]);
        }
        return names;
    }

    @Override
    public String getGameRuleValue(String rule) {
        return WorldNatives.gameRule(name, BasaltGameRule.modernName(rule));
    }

    @Override
    public boolean setGameRuleValue(String rule, String value) {
        return WorldNatives.setGameRule(name, BasaltGameRule.modernName(rule), value);
    }

    @Override
    public boolean isGameRule(String rule) {
        return WorldNatives.gameRule(name, BasaltGameRule.modernName(rule)) != null;
    }

    @Override
    @SuppressWarnings({"unchecked", "rawtypes"})
    public <T> T getGameRuleValue(GameRule<T> rule) {
        if (rule instanceof BasaltGameRule.Legacy legacy) {
            Object modern = getGameRuleValue(legacy.modern);
            return modern == null ? null : (T) legacy.fromModern.apply(modern);
        }
        String value = WorldNatives.gameRule(name, rule.getKey().getKey());
        if (value == null) {
            return null;
        }
        return (T) (rule.getType() == Boolean.class ? (Object) Boolean.valueOf(value) : (Object) Integer.valueOf(value));
    }

    @Override
    @SuppressWarnings({"unchecked", "rawtypes"})
    public <T> T getGameRuleDefault(GameRule<T> rule) {
        return rule.getDefaultValue();
    }

    @Override
    @SuppressWarnings({"unchecked", "rawtypes"})
    public <T> boolean setGameRule(GameRule<T> rule, T value) {
        if (rule instanceof BasaltGameRule.Legacy legacy) {
            return setGameRule(legacy.modern, legacy.toModern.apply(value));
        }
        return WorldNatives.setGameRule(name, rule.getKey().getKey(), String.valueOf(value));
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltWorld world && world.name.equals(name);
    }

    @Override
    public int hashCode() {
        return name.hashCode();
    }

    @Override
    public String toString() {
        return "BasaltWorld{name=" + name + "}";
    }

    @Override
    public org.bukkit.persistence.PersistentDataContainer getPersistentDataContainer() {
        return org.basaltmc.bukkit.persistence.PdcStores.world(name);
    }

    private final MetadataHolder metadata = new MetadataHolder();

    @Override
    public void setMetadata(String name, org.bukkit.metadata.MetadataValue value) {
        metadata.set(name, value);
    }

    @Override
    public java.util.List<org.bukkit.metadata.MetadataValue> getMetadata(String name) {
        return metadata.get(name);
    }

    @Override
    public boolean hasMetadata(String name) {
        return metadata.has(name);
    }

    @Override
    public void removeMetadata(String name, org.bukkit.plugin.Plugin plugin) {
        metadata.remove(name, plugin);
    }

    @Override
    public org.bukkit.util.StructureSearchResult locateNearestStructure(Location origin,
            org.bukkit.generator.structure.Structure structure, int radius, boolean findUnexplored) {
        int[] at = MiscNatives.locateStructure(name, structure.getKey().toString(), origin.getBlockX(),
            origin.getBlockZ(), Math.max(radius, 1));
        if (at == null) {
            return null;
        }
        Location found = new Location(this, at[0], 0, at[1]);
        return new org.bukkit.util.StructureSearchResult() {
            @Override
            public org.bukkit.generator.structure.Structure getStructure() {
                return structure;
            }

            @Override
            public Location getLocation() {
                return found;
            }
        };
    }

    @Override
    public Location locateNearestStructure(Location origin, org.bukkit.StructureType structureType, int radius,
            boolean findUnexplored) {
        int[] at = MiscNatives.locateStructure(name, "minecraft:" + structureType.getName(), origin.getBlockX(),
            origin.getBlockZ(), Math.max(radius, 1));
        return at == null ? null : new Location(this, at[0], 0, at[1]);
    }

    @Override
    public org.bukkit.util.BiomeSearchResult locateNearestBiome(Location origin, int radius, int horizontalInterval,
            int verticalInterval, org.bukkit.block.Biome... biomes) {
        String[] wanted = new String[biomes.length];
        for (int i = 0; i < biomes.length; i++) {
            wanted[i] = biomes[i].getKey().toString();
        }
        int[] at = MiscNatives.locateBiome(name, wanted, origin.getBlockX(), origin.getBlockY(), origin.getBlockZ(),
            radius, horizontalInterval);
        if (at == null) {
            return null;
        }
        Location found = new Location(this, at[0], at[1], at[2]);
        org.bukkit.block.Biome biome = getBiome(at[0], at[1], at[2]);
        return new org.bukkit.util.BiomeSearchResult() {
            @Override
            public org.bukkit.block.Biome getBiome() {
                return biome;
            }

            @Override
            public Location getLocation() {
                return found;
            }
        };
    }

    @Override
    public org.bukkit.util.RayTraceResult rayTraceBlocks(io.papermc.paper.math.Position start,
            org.bukkit.util.Vector direction, double maxDistance, org.bukkit.FluidCollisionMode fluidCollisionMode,
            boolean ignorePassableBlocks, java.util.function.Predicate<? super org.bukkit.block.Block> canCollide) {
        return org.basaltmc.bukkit.misc.RayTracing.blocks(this, new org.bukkit.util.Vector(start.x(), start.y(), start.z()),
            direction, maxDistance, fluidCollisionMode, ignorePassableBlocks, canCollide);
    }

    @Override
    public org.bukkit.util.RayTraceResult rayTraceEntities(io.papermc.paper.math.Position start,
            org.bukkit.util.Vector direction, double maxDistance, double raySize,
            java.util.function.Predicate<? super org.bukkit.entity.Entity> filter) {
        return org.basaltmc.bukkit.misc.RayTracing.entities(this, new org.bukkit.util.Vector(start.x(), start.y(), start.z()),
            direction, maxDistance, raySize, filter);
    }

    @Override
    public org.bukkit.util.RayTraceResult rayTrace(io.papermc.paper.math.Position start,
            org.bukkit.util.Vector direction, double maxDistance, org.bukkit.FluidCollisionMode fluidCollisionMode,
            boolean ignorePassableBlocks, double raySize,
            java.util.function.Predicate<? super org.bukkit.entity.Entity> filter,
            java.util.function.Predicate<? super org.bukkit.block.Block> canCollide) {
        return org.basaltmc.bukkit.misc.RayTracing.both(this, new org.bukkit.util.Vector(start.x(), start.y(), start.z()),
            direction, maxDistance, fluidCollisionMode, ignorePassableBlocks, raySize, filter, canCollide);
    }

    @Override
    public boolean hasCollisionsIn(org.bukkit.util.BoundingBox box) {
        for (int x = (int) Math.floor(box.getMinX()); x <= (int) Math.floor(box.getMaxX()); x++) {
            for (int y = (int) Math.floor(box.getMinY()); y <= (int) Math.floor(box.getMaxY()); y++) {
                for (int z = (int) Math.floor(box.getMinZ()); z <= (int) Math.floor(box.getMaxZ()); z++) {
                    double[] boxes = org.basaltmc.bukkit.misc.Shapes.boxes(WorldNatives.state(name, x, y, z, false));
                    for (int i = 0; i + 5 < boxes.length; i += 6) {
                        if (box.overlaps(new org.bukkit.util.BoundingBox(x + boxes[i], y + boxes[i + 1], z + boxes[i + 2],
                                x + boxes[i + 3], y + boxes[i + 4], z + boxes[i + 5]))) {
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }

    @Override
    public org.bukkit.util.RayTraceResult rayTrace(
            java.util.function.Consumer<io.papermc.paper.raytracing.PositionedRayTraceConfigurationBuilder> builder) {
        org.basaltmc.bukkit.misc.RayTraceBuilder configured = new org.basaltmc.bukkit.misc.RayTraceBuilder();
        builder.accept(configured);
        return configured.run();
    }

    @Override
    public org.bukkit.boss.DragonBattle getEnderDragonBattle() {
        return org.basaltmc.bukkit.misc.BasaltDragonBattle.of(this);
    }
}
