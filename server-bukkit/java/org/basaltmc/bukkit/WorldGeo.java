package org.basaltmc.bukkit;

import io.papermc.paper.block.fluid.FluidData;
import io.papermc.paper.entity.poi.PoiSearchResult;
import io.papermc.paper.entity.poi.PoiType;
import io.papermc.paper.world.chunk.ChunkGenerationState;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.function.Predicate;
import org.bukkit.Chunk;
import org.bukkit.FeatureFlag;
import org.bukkit.GameEvent;
import org.bukkit.Location;
import org.bukkit.NamespacedKey;
import org.bukkit.Raid;
import org.bukkit.Registry;
import org.bukkit.TreeType;
import org.bukkit.block.Biome;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Player;
import org.bukkit.entity.SpawnCategory;
import org.bukkit.generator.BiomeProvider;
import org.bukkit.generator.structure.GeneratedStructure;
import org.bukkit.generator.structure.Structure;
import org.bukkit.plugin.Plugin;
import org.bukkit.util.BoundingBox;
import org.bukkit.util.Vector;

/** The part of the World API that reads or sets how the world around a position is made and ruled. */
public abstract class WorldGeo extends WorldExtras {
    private final Map<Plugin, Set<Long>> tickets = new LinkedHashMap<>();

    private static int slot(SpawnCategory category) {
        return switch (category) {
            case MONSTER -> 0;
            case ANIMAL -> 1;
            case AMBIENT -> 2;
            case AXOLOTL -> 3;
            case WATER_UNDERGROUND_CREATURE -> 4;
            case WATER_ANIMAL -> 5;
            case WATER_AMBIENT -> 6;
            default -> 7;
        };
    }

    private int[] tunables() {
        int[] values = WorldEnvNatives.spawnTunables(getName());
        if (values == null) {
            throw new IllegalStateException("The world " + getName() + " is not loaded");
        }
        return values;
    }

    @Override
    public int getSpawnLimit(SpawnCategory category) {
        int value = tunables()[slot(category)];
        return value >= 0 ? value : Spawning.limit(category);
    }

    @Override
    public void setSpawnLimit(SpawnCategory category, int limit) {
        int[] values = tunables();
        values[slot(category)] = limit < 0 ? -1 : limit;
        WorldEnvNatives.setSpawnTunables(getName(), values);
    }

    @Override
    public long getTicksPerSpawns(SpawnCategory category) {
        int value = tunables()[8 + slot(category)];
        return value >= 0 ? value : Spawning.ticksPer(category);
    }

    @Override
    public void setTicksPerSpawns(SpawnCategory category, int ticks) {
        int[] values = tunables();
        values[8 + slot(category)] = ticks < 0 ? -1 : ticks;
        WorldEnvNatives.setSpawnTunables(getName(), values);
    }

    @Override
    public boolean getAllowMonsters() {
        return tunables()[16] != 0;
    }

    @Override
    public boolean getAllowAnimals() {
        return tunables()[17] != 0;
    }

    @Override
    public void setSpawnFlags(boolean allowMonsters, boolean allowAnimals) {
        int[] values = tunables();
        values[16] = allowMonsters ? 1 : 0;
        values[17] = allowAnimals ? 1 : 0;
        WorldEnvNatives.setSpawnTunables(getName(), values);
    }

    @Override
    public void setAllowMonsterSpawning(boolean allow) {
        setSpawnFlags(allow, getAllowAnimals());
    }

    @Override
    public int getViewDistance() {
        return WorldEnvNatives.viewDistance(getName(), 0);
    }

    @Override
    public void setViewDistance(int distance) {
        WorldEnvNatives.viewDistance(getName(), Math.max(2, distance));
    }

    @Override
    public int getSendViewDistance() {
        return getViewDistance();
    }

    @Override
    public void setSendViewDistance(int distance) {
        setViewDistance(distance);
    }

    @Override
    public void setSimulationDistance(int distance) {
        throw Unsupported.hit("World#setSimulationDistance");
    }

    @Override
    public boolean isVoidDamageEnabled() {
        return true;
    }

    @Override
    public float getVoidDamageAmount() {
        return 4.0F;
    }

    @Override
    public double getVoidDamageMinBuildHeightOffset() {
        return -64.0;
    }

    @Override
    public void setVoidDamageEnabled(boolean enabled) {
        throw Unsupported.hit("World#setVoidDamageEnabled");
    }

    @Override
    public void setVoidDamageAmount(float amount) {
        throw Unsupported.hit("World#setVoidDamageAmount");
    }

    @Override
    public void setVoidDamageMinBuildHeightOffset(double offset) {
        throw Unsupported.hit("World#setVoidDamageMinBuildHeightOffset");
    }

    @Override
    public int getSeaLevel() {
        return WorldEnvNatives.worldFacts(getName())[0];
    }

    @Override
    public int getWeatherDuration() {
        return WorldEnvNatives.worldFacts(getName())[1];
    }

    @Override
    public void setWeatherDuration(int ticks) {
        int[] facts = WorldEnvNatives.worldFacts(getName());
        WorldEnvNatives.setWeatherTimes(getName(), Math.max(ticks, 0), facts[2], facts[3]);
    }

    @Override
    public int getThunderDuration() {
        return WorldEnvNatives.worldFacts(getName())[2];
    }

    @Override
    public void setThunderDuration(int ticks) {
        int[] facts = WorldEnvNatives.worldFacts(getName());
        WorldEnvNatives.setWeatherTimes(getName(), facts[1], Math.max(ticks, 0), facts[3]);
    }

    @Override
    public int getClearWeatherDuration() {
        return WorldEnvNatives.worldFacts(getName())[3];
    }

    @Override
    public void setClearWeatherDuration(int ticks) {
        int[] facts = WorldEnvNatives.worldFacts(getName());
        WorldEnvNatives.setWeatherTimes(getName(), facts[1], facts[2], Math.max(ticks, 0));
    }

    @Override
    public double getTemperature(int x, int y, int z) {
        return WorldEnvNatives.temperature(getName(), x, y, z);
    }

    @Override
    public double getHumidity(int x, int y, int z) {
        throw Unsupported.hit("World#getHumidity");
    }

    @Override
    public Collection<Player> getPlayersSeeingChunk(int x, int z) {
        List<Player> out = new ArrayList<>();
        int view = getViewDistance();
        for (Player player : getPlayers()) {
            Location at = player.getLocation();
            if (Math.abs((at.getBlockX() >> 4) - x) <= view && Math.abs((at.getBlockZ() >> 4) - z) <= view) {
                out.add(player);
            }
        }
        return out;
    }

    @Override
    public Collection<Player> getPlayersSeeingChunk(Chunk chunk) {
        return getPlayersSeeingChunk(chunk.getX(), chunk.getZ());
    }

    @Override
    public Path getWorldPath() {
        return BasaltServer.instance().getWorldContainer().toPath().resolve(getName());
    }

    @Override
    public Set<FeatureFlag> getFeatureFlags() {
        return Set.of(FeatureFlag.VANILLA);
    }

    @Override
    public boolean isChunkForceLoaded(int x, int z) {
        int[] forced = WorldEnvNatives.forcedChunks(getName());
        for (int i = 0; forced != null && i + 1 < forced.length; i += 2) {
            if (forced[i] == x && forced[i + 1] == z) {
                return true;
            }
        }
        return false;
    }

    @Override
    public void setChunkForceLoaded(int x, int z, boolean forced) {
        WorldEnvNatives.setChunkForced(getName(), x, z, forced);
    }

    @Override
    public Collection<Chunk> getForceLoadedChunks() {
        List<Chunk> out = new ArrayList<>();
        int[] forced = WorldEnvNatives.forcedChunks(getName());
        for (int i = 0; forced != null && i + 1 < forced.length; i += 2) {
            out.add(getChunkAt(forced[i], forced[i + 1], false));
        }
        return out;
    }

    private void hold(Plugin plugin) {
        Set<Long> keys = tickets.getOrDefault(plugin, Set.of());
        String token = "plugin:" + plugin.getName();
        if (keys.isEmpty()) {
            WorldNatives.releaseChunks(getName(), token);
            return;
        }
        int[] flat = new int[keys.size() * 2];
        int at = 0;
        for (long key : keys) {
            flat[at++] = (int) (key >> 32);
            flat[at++] = (int) key;
        }
        WorldNatives.holdChunks(getName(), token, flat);
    }

    private static long key(int x, int z) {
        return ((long) x << 32) | (z & 0xFFFFFFFFL);
    }

    @Override
    public synchronized boolean addPluginChunkTicket(int x, int z, Plugin plugin) {
        boolean added = tickets.computeIfAbsent(plugin, ignored -> new HashSet<>()).add(key(x, z));
        if (added) {
            hold(plugin);
            WorldNatives.loadChunk(getName(), x, z, true);
        }
        return added;
    }

    @Override
    public synchronized boolean removePluginChunkTicket(int x, int z, Plugin plugin) {
        Set<Long> keys = tickets.get(plugin);
        boolean removed = keys != null && keys.remove(key(x, z));
        if (removed) {
            hold(plugin);
        }
        return removed;
    }

    @Override
    public synchronized void removePluginChunkTickets(Plugin plugin) {
        if (tickets.remove(plugin) != null) {
            WorldNatives.releaseChunks(getName(), "plugin:" + plugin.getName());
        }
    }

    @Override
    public synchronized Collection<Plugin> getPluginChunkTickets(int x, int z) {
        List<Plugin> out = new ArrayList<>();
        for (Map.Entry<Plugin, Set<Long>> entry : tickets.entrySet()) {
            if (entry.getValue().contains(key(x, z))) {
                out.add(entry.getKey());
            }
        }
        return out;
    }

    @Override
    public synchronized Map<Plugin, Collection<Chunk>> getPluginChunkTickets() {
        Map<Plugin, Collection<Chunk>> out = new LinkedHashMap<>();
        for (Map.Entry<Plugin, Set<Long>> entry : tickets.entrySet()) {
            List<Chunk> chunks = new ArrayList<>();
            for (long key : entry.getValue()) {
                chunks.add(getChunkAt((int) (key >> 32), (int) key, false));
            }
            out.put(entry.getKey(), chunks);
        }
        return out;
    }

    @Override
    public Collection<Chunk> getIntersectingChunks(BoundingBox box) {
        List<Chunk> out = new ArrayList<>();
        for (int x = (int) Math.floor(box.getMinX()) >> 4; x <= (int) Math.floor(box.getMaxX()) >> 4; x++) {
            for (int z = (int) Math.floor(box.getMinZ()) >> 4; z <= (int) Math.floor(box.getMaxZ()) >> 4; z++) {
                out.add(getChunkAt(x, z, false));
            }
        }
        return out;
    }

    @Override
    public void getChunksAtAsync(int minX, int minZ, int maxX, int maxZ, boolean urgent, Runnable done) {
        List<CompletableFuture<Chunk>> pending = new ArrayList<>();
        for (int x = minX; x <= maxX; x++) {
            for (int z = minZ; z <= maxZ; z++) {
                pending.add(getChunkAtAsync(x, z, true, urgent));
            }
        }
        CompletableFuture.allOf(pending.toArray(new CompletableFuture<?>[0])).thenRun(done);
    }

    private static ChunkGenerationState stateOf(int status) {
        return status <= 0 ? ChunkGenerationState.NOT_GENERATED : status == 1 ? ChunkGenerationState.PARTIAL
            : status == 2 ? ChunkGenerationState.OUTDATED : ChunkGenerationState.GENERATED;
    }

    @Override
    public ChunkGenerationState getChunkGenerationState(int x, int z) {
        return stateOf(WorldNatives.chunkStatus(getName(), x, z));
    }

    @Override
    public CompletableFuture<ChunkGenerationState> getChunkGenerationStateAsync(int x, int z) {
        return CompletableFuture.completedFuture(getChunkGenerationState(x, z));
    }

    @Override
    public boolean generateTree(Location location, TreeType type) {
        String sapling = switch (type) {
            case TREE, BIG_TREE, SWAMP -> "oak_sapling";
            case REDWOOD, TALL_REDWOOD, MEGA_REDWOOD, MEGA_PINE -> "spruce_sapling";
            case BIRCH, TALL_BIRCH -> "birch_sapling";
            case JUNGLE, SMALL_JUNGLE, COCOA_TREE, JUNGLE_BUSH -> "jungle_sapling";
            case ACACIA -> "acacia_sapling";
            case DARK_OAK -> "dark_oak_sapling";
            case CHERRY -> "cherry_sapling";
            case PALE_OAK, PALE_OAK_CREAKING -> "pale_oak_sapling";
            default -> throw Unsupported.hit("World#generateTree " + type);
        };
        Blocks.flushAll();
        return WorldEnvNatives.growTree(getName(), location.getBlockX(), location.getBlockY(), location.getBlockZ(),
            sapling);
    }

    @Override
    public boolean generateTree(Location location, TreeType type, org.bukkit.BlockChangeDelegate delegate) {
        throw Unsupported.hit("World#generateTree with a block change delegate");
    }

    @Override
    public boolean lineOfSightExists(Location from, Location to) {
        Vector direction = to.toVector().subtract(from.toVector());
        double distance = direction.length();
        return distance == 0.0 || rayTraceBlocks(from, direction.normalize(), distance,
            org.bukkit.FluidCollisionMode.NEVER, true) == null;
    }

    @Override
    public FluidData getFluidData(int x, int y, int z) {
        Blocks.flushAll();
        String row = WorldEnvNatives.fluidAt(getName(), x, y, z);
        return new BasaltFluidData((BasaltWorld) this, x, y, z, row == null ? "empty|0|0|0" : row);
    }

    @Override
    public Location findLightningRod(Location location) {
        Location target = findLightningTarget(location);
        return target != null && target.getBlock().getType() == org.bukkit.Material.LIGHTNING_ROD ? target : null;
    }

    @Override
    public Location findLightningTarget(Location location) {
        Blocks.flushAll();
        int[] at = WorldEnvNatives.lightningTarget(getName(), location.getBlockX(), location.getBlockZ());
        return at == null ? null : new Location(this, at[0], at[1], at[2]);
    }

    @Override
    public void sendGameEvent(Entity source, GameEvent event, Vector position) {
        WorldEnvNatives.gameEvent(getName(), event.getKey().asString(), position.getX(), position.getY(),
            position.getZ(), source == null ? null : source.getUniqueId().toString());
    }

    @Override
    public BiomeProvider vanillaBiomeProvider() {
        BasaltWorld self = (BasaltWorld) this;
        return new BiomeProvider() {
            @Override
            public Biome getBiome(org.bukkit.generator.WorldInfo info, int x, int y, int z) {
                return self.getBiome(x, y, z);
            }

            @Override
            public List<Biome> getBiomes(org.bukkit.generator.WorldInfo info) {
                List<Biome> out = new ArrayList<>();
                Registry.BIOME.forEach(out::add);
                return out;
            }
        };
    }

    @Override
    public Collection<GeneratedStructure> getStructures(int chunkX, int chunkZ) {
        return BasaltStructures.in((BasaltWorld) this, chunkX, chunkZ, null);
    }

    @Override
    public Collection<GeneratedStructure> getStructures(int chunkX, int chunkZ, Structure structure) {
        return BasaltStructures.in((BasaltWorld) this, chunkX, chunkZ, structure);
    }

    @Override
    public boolean hasStructureAt(io.papermc.paper.math.Position position, Structure structure) {
        int x = position.blockX();
        int y = position.blockY();
        int z = position.blockZ();
        for (GeneratedStructure start : getStructures(x >> 4, z >> 4, structure)) {
            if (start.getBoundingBox().contains(x + 0.5, y + 0.5, z + 0.5)) {
                return true;
            }
        }
        return false;
    }

    @Override
    public Raid getRaid(int id) {
        for (Raid raid : getRaids()) {
            if (raid.getId() == id) {
                return raid;
            }
        }
        return null;
    }

    @Override
    public List<Raid> getRaids() {
        List<Raid> out = new ArrayList<>();
        String[] rows = WorldEnvNatives.raids(getName());
        for (String row : rows == null ? new String[0] : rows) {
            out.add(new BasaltRaid((BasaltWorld) this, row));
        }
        return out;
    }

    @Override
    public Raid locateNearestRaid(Location location, int radius) {
        Raid best = null;
        double bestDistance = (double) radius * radius;
        for (Raid raid : getRaids()) {
            double distance = raid.getLocation().distanceSquared(location);
            if (distance <= bestDistance) {
                best = raid;
                bestDistance = distance;
            }
        }
        return best;
    }

    private List<PoiSearchResult> pois(Location location, Predicate<PoiType> types, int radius,
        PoiType.Occupancy occupancy) {
        int mode = occupancy == PoiType.Occupancy.HAS_SPACE ? 0 : occupancy == PoiType.Occupancy.IS_OCCUPIED ? 1 : 2;
        String[] rows = WorldEnvNatives.pointsOfInterest(getName(), new String[0], location.getBlockX(),
            location.getBlockY(), location.getBlockZ(), radius, mode);
        List<PoiSearchResult> out = new ArrayList<>();
        for (String row : rows == null ? new String[0] : rows) {
            String[] part = row.split("\\|");
            PoiType type = Registry.POINT_OF_INTEREST_TYPE.get(NamespacedKey.fromString(part[0]));
            if (type == null || !types.test(type)) {
                continue;
            }
            Location at = new Location(this, Integer.parseInt(part[1]), Integer.parseInt(part[2]),
                Integer.parseInt(part[3]));
            out.add(new PoiSearchResult() {
                @Override
                public PoiType poiType() {
                    return type;
                }

                @Override
                public Location location() {
                    return at;
                }
            });
        }
        out.sort(java.util.Comparator.comparingDouble(result -> result.location().distanceSquared(location)));
        return out;
    }

    @Override
    public Location locateNearestPoi(Location location, PoiType type, int radius, PoiType.Occupancy occupancy) {
        List<PoiSearchResult> found = pois(location, candidate -> candidate.getKey().equals(type.getKey()), radius,
            occupancy);
        return found.isEmpty() ? null : found.get(0).location();
    }

    @Override
    public List<PoiSearchResult> locateAllPoiInRange(Location location, Predicate<PoiType> types, int radius,
        PoiType.Occupancy occupancy) {
        return pois(location, types, radius, occupancy);
    }

    @Override
    public org.bukkit.util.StructureSearchResult locateNearestStructure(Location origin,
        org.bukkit.generator.structure.StructureType type, int radius, boolean findUnexplored) {
        throw Unsupported.hit("World#locateNearestStructure by structure type");
    }
}
