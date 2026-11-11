package org.basaltmc.bukkit;

public final class LeftoverNatives {
    private LeftoverNatives() {}

    public static native String[] structureIds();
    public static native String[] stopwatchIds();
    public static native double stopwatch(String operation, String id);
    /** Feet position in the target world of the nether portal an entity enters, built when none is near; null if none. */
    public static native double[] portalExit(String from, String to, double x, double y, double z, boolean alongX);
    public static native String[] functionIds();
    public static native String[] functionTagIds();
    /** A file of the enabled data packs by its full name, top pack first; null when no pack has it. */
    public static native byte[] resourceRead(String name);

    public static native String[] resourcesUnder(String prefix);

    /** The id of the pack a file comes from, or null. */
    public static native String resourcePack(String name);

    public static native byte[] structureRead(String id);
    public static native boolean structureStore(String id, byte[] nbt, boolean toDisk);
    public static native boolean structureRemove(String id, boolean fromDisk);
    public static native String structureFile(String id);
    public static native byte[] structureCapture(String world, int x, int y, int z, int sizeX, int sizeY, int sizeZ,
        boolean entities);
    /** Size, flat block ints (x, y, z, state), block entity bytes, flat jigsaw ints, flat jigsaw strings. */
    public static native Object[] templateScan(byte[] nbt, int x, int y, int z, int rotation, int mirror, int pivotX,
        int pivotZ, String block, boolean absolute);
    /** Unnamed NBT of a data type brought from one world version to another, or null when it has no native fixer. */
    public static native byte[] datafix(String type, byte[] nbt, int fromVersion, int toVersion);
    /** The one vanilla step registered under the name at the version key, run on unnamed NBT; null when it cannot run. */
    public static native byte[] datafixStep(String type, String step, int versionKey, byte[] nbt);
    /** The first step between two versions that cannot run natively for the type, or null when every step can. */
    public static native String datafixMissing(String type, int fromVersion, int toVersion);
    public static native boolean datafixSupports(String type);
    /** The 1.13 name of a pre-1.13 `block << 4 | data` id followed by its property keys and values in pairs. */
    public static native String[] datafixLegacyState(int id);
    /** Min y, height, sea level, default block and fluid state ids, legacy random flag, no-mobs flag; null when unknown. */
    public static native int[] terrainShape(String settings);
    /** A router function of the noise settings at a block; NaN when the settings or function are unknown. */
    public static native double terrainDensityTree(String settings, long seed, String json, int x, int y, int z);
    public static native double terrainDensity(String settings, long seed, String function, int x, int y, int z);
    public static native double terrainNoise(String settings, long seed, String noise, double x, double y, double z);
    /** Smallest and largest value of a named noise, null when unknown. */
    public static native double[] terrainNoiseRange(String settings, long seed, String noise);
    /** A game data noise seeded from a random source state; a handle, or -1. */
    public static native long noiseCreate(String noise, boolean legacyRandom, long low, long high, boolean legacyNether);
    public static native long noiseCreateWith(double baseAmplitude, int baseOctave, int octaveCount, int normalize,
        double[] modifiers, boolean legacyRandom, long low, long high, boolean legacyNether);
    public static native double noiseSample(long handle, double x, double y, double z);
    public static native double[] noiseHandleRange(long handle);
    public static native void noiseRelease(long handle);
    /** One above the topmost base terrain block, Integer.MIN_VALUE when the settings are unknown. */
    public static native int terrainHeight(String settings, long seed, int x, int z, boolean oceanFloor);
    public static native int[] terrainColumn(String settings, long seed, int x, int z);
    public static native boolean structurePlace(byte[] nbt, String world, int x, int y, int z, int rotation,
        int mirror, float integrity, long seed, boolean entities, int palette);
    public static native byte[] registryData(String registry, String id);
    public static native String[] entityDefaults(String type);
    public static native double humidity(String world, int x, int y, int z);
    public static native long[] climate(String world, int quartX, int quartY, int quartZ);
    public static native boolean dragonAction(String world, String action, int value);

    public static native boolean dragonBar(String world, String action, String text, int value);
    public static native boolean windowProperty(String uuid, int id, int value);
    public static native String[] applyLoot(String[] stack, String world, String functions, double x, double y, double z, long seed);
    public static native String[] enchantItem(String[] stack, String world, String options, int levels, long seed);
    public static native byte[] explorerMap(String world, double x, double y, double z, String[] destination,
        String decoration, int zoom, int radius, boolean skipExisting);
}
