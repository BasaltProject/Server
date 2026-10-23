package org.basaltmc.bukkit.worldgen;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.ConcurrentHashMap;
import org.basaltmc.bukkit.BasaltWorld;
import org.basaltmc.bukkit.Natives;
import org.basaltmc.bukkit.Registries;
import org.bukkit.block.Biome;
import org.bukkit.generator.BiomeProvider;
import org.bukkit.generator.BlockPopulator;
import org.bukkit.generator.ChunkGenerator;

/** The worlds whose chunks a plugin's generator makes, and the populators that decorate them. */
public final class CustomGenerators {
    private static final Map<String, Entry> kEntries = new ConcurrentHashMap<>();
    private static boolean subscribed;

    private CustomGenerators() {}

    private static final class Entry {
        final BasaltWorld world;
        volatile ChunkGenerator generator;
        volatile BiomeProvider biomes;
        final List<BlockPopulator> populators = new java.util.concurrent.CopyOnWriteArrayList<>();
        final Object lock = new Object();

        Entry(BasaltWorld world) {
            this.world = world;
        }
    }

    private static synchronized Entry entryOf(BasaltWorld world) {
        Entry entry = kEntries.computeIfAbsent(world.getName(), key -> new Entry(world));
        if (!subscribed) {
            subscribed = Natives.subscribe("world.chunk_load", 0);
        }
        return entry;
    }

    /** A chunk data of the world's size, for a generator that makes its own. */
    public static ChunkGenerator.ChunkData newChunkData(org.bukkit.World world) {
        return new BasaltChunkData(world.getMinHeight(), world.getMaxHeight(),
            new CellBiomes(world.getMinHeight(), world.getMaxHeight(), 0));
    }

    /** Puts the generator in charge of the chunks of a void world; false when the server cannot hand them over. */
    public static synchronized boolean install(BasaltWorld world, ChunkGenerator generator, BiomeProvider biomes) {
        BiomeProvider provider = biomes != null ? biomes : generator.getDefaultBiomeProvider(world);
        Entry entry = entryOf(world);
        entry.generator = generator;
        entry.biomes = provider;
        if (!org.basaltmc.bukkit.WorldEnvNatives.useGenerator(world.getName())) {
            entry.generator = null;
            return false;
        }
        List<BlockPopulator> defaults = generator.getDefaultPopulators(world);
        if (defaults != null) {
            entry.populators.addAll(defaults);
        }
        return true;
    }

    public static ChunkGenerator generatorOf(String world) {
        Entry entry = kEntries.get(world);
        return entry == null ? null : entry.generator;
    }

    public static BiomeProvider biomesOf(String world) {
        Entry entry = kEntries.get(world);
        return entry == null ? null : entry.biomes;
    }

    public static List<BlockPopulator> populatorsOf(BasaltWorld world) {
        return entryOf(world).populators;
    }

    private static boolean isLegacy(ChunkGenerator generator) {
        try {
            return generator.getClass().getMethod("generateChunkData", org.bukkit.World.class, Random.class, int.class,
                int.class, ChunkGenerator.BiomeGrid.class).getDeclaringClass() != ChunkGenerator.class;
        } catch (NoSuchMethodException e) {
            return false;
        }
    }

    private static long chunkSeed(long seed, int chunkX, int chunkZ) {
        return (long) chunkX * 341873128712L + (long) chunkZ * 132897987541L ^ seed;
    }

    /** The block state ids of every section from the lowest, then the biome cells; called off the tick thread. */
    public static int[] fill(String name, int chunkX, int chunkZ) {
        Entry entry = kEntries.get(name);
        if (entry == null || entry.generator == null) {
            return null;
        }
        BasaltWorld world = entry.world;
        int minY = world.getMinHeight();
        int maxY = world.getMaxHeight();
        String[] known = Registries.biomeIds();
        int plains = Math.max(0, java.util.Arrays.asList(known).indexOf("minecraft:plains"));
        CellBiomes grid = new CellBiomes(minY, maxY, plains);
        BasaltChunkData data = new BasaltChunkData(minY, maxY, grid);
        Object guard = entry.generator.isParallelCapable() ? new Object() : entry.lock;
        synchronized (guard) {
            Random random = new Random(chunkSeed(0L, chunkX, chunkZ));
            ChunkGenerator.ChunkData legacy = isLegacy(entry.generator)
                ? entry.generator.generateChunkData(world, random, chunkX, chunkZ, grid) : null;
            if (legacy instanceof BasaltChunkData own) {
                data = own;
            } else {
                if (entry.biomes != null) {
                    for (int y = minY; y < maxY; y += 4) {
                        for (int z = 0; z < 16; z += 4) {
                            for (int x = 0; x < 16; x += 4) {
                                Biome biome = entry.biomes.getBiome(world, chunkX * 16 + x, y, chunkZ * 16 + z);
                                if (biome != null) {
                                    grid.set(x, y, z, biome);
                                }
                            }
                        }
                    }
                }
                random.setSeed(chunkSeed(0L, chunkX, chunkZ));
                entry.generator.generateNoise(world, random, chunkX, chunkZ, data);
                random.setSeed(chunkSeed(0L, chunkX, chunkZ));
                entry.generator.generateSurface(world, random, chunkX, chunkZ, data);
                random.setSeed(chunkSeed(0L, chunkX, chunkZ));
                entry.generator.generateBedrock(world, random, chunkX, chunkZ, data);
                random.setSeed(chunkSeed(0L, chunkX, chunkZ));
                entry.generator.generateCaves(world, random, chunkX, chunkZ, data);
            }
        }
        int[] blocks = data.flat();
        int[] cells = grid.cells();
        int[] packed = new int[2 + blocks.length + cells.length];
        packed[0] = blocks.length;
        packed[1] = cells.length;
        System.arraycopy(blocks, 0, packed, 2, blocks.length);
        System.arraycopy(cells, 0, packed, 2 + blocks.length, cells.length);
        return packed;
    }

    /** Runs the populators of a world on a chunk it has just made. */
    public static void chunkLoaded(Object[] values) {
        if (values.length < 4 || !(values[3] instanceof Boolean generated) || !generated) {
            return;
        }
        String name = String.valueOf(values[0]);
        Entry entry = kEntries.get(name);
        if (entry == null || entry.populators.isEmpty()) {
            return;
        }
        int chunkX = ((Number) values[1]).intValue();
        int chunkZ = ((Number) values[2]).intValue();
        BasaltWorld world = entry.world;
        Random seeder = new Random(world.getSeed());
        long xRand = seeder.nextLong() / 2L * 2L + 1L;
        long zRand = seeder.nextLong() / 2L * 2L + 1L;
        for (BlockPopulator populator : new ArrayList<>(entry.populators)) {
            Random random = new Random((long) chunkX * xRand + (long) chunkZ * zRand ^ world.getSeed());
            populator.populate(world, random, world.getChunkAt(chunkX, chunkZ, false));
            populator.populate(world, random, chunkX, chunkZ, BasaltLimitedRegion.of(world, chunkX, chunkZ, 16));
        }
    }
}
