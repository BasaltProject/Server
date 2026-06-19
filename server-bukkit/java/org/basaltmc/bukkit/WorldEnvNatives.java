package org.basaltmc.bukkit;

public final class WorldEnvNatives {
    private WorldEnvNatives() {}

    public static native void sound(String world, String sound, String category, double x, double y, double z,
        float volume, float pitch, long seed);

    public static native void particle(String world, String type, double x, double y, double z, int count,
        double offsetX, double offsetY, double offsetZ, double speedX, double speedY, double speedZ, int randomization,
        boolean force, double[] data, String item,
        String receiver);

    public static native void levelEvent(String world, int type, int x, int y, int z, int data, boolean global);

    public static native void blockEvent(String world, int x, int y, int z, int first, int second);

    /** Centre x and z, size, target size, lerp ticks, damage, buffer, warning blocks, warning seconds. */
    public static native double[] border(String world);

    public static native void setBorder(String world, double[] values, double seconds);

    public static native int playerCount(String world);

    public static native int blockEntityCount(String world);

    /** Item id, count and components SNBT per drop; null when the block gives no answer. */
    public static native String[] blockDrops(String world, int x, int y, int z, String tool, String[] enchantments,
        int[] levels, boolean byEntity);

    public static native boolean breakBlock(String world, int x, int y, int z, String tool, String[] enchantments,
        int[] levels, boolean drops);

    public static native boolean boneMeal(String world, int x, int y, int z);

    public static native int signal(String world, int x, int y, int z, int face, boolean direct);

    public static native boolean survives(String world, int x, int y, int z, int state);

    public static native double temperature(String world, int x, int y, int z);

    public static native double breakSpeed(String world, String player, int x, int y, int z);

    /** Per structure start: name, chunk x and z, box, then the boxes of its pieces, joined by bars. */
    public static native String[] structureStarts(String world, int chunkX, int chunkZ);

    public static native String[] raids(String world);

    public static native boolean setRaid(String world, int id, int omenLevel, int totalGroups);

    /** Fluid name, source flag, level and falling flag joined by bars. */
    public static native String fluidAt(String world, int x, int y, int z);

    public static native int[] lightningTarget(String world, int x, int z);

    public static native void gameEvent(String world, String event, double x, double y, double z, String source);

    public static native boolean growTree(String world, int x, int y, int z, String sapling);

    public static native String[] pointsOfInterest(String world, String[] types, int x, int y, int z, int radius,
        int occupancy);

    /** Eight limits, eight ticks per spawn, then the monster and animal flags. */
    public static native int[] spawnTunables(String world);

    public static native void setSpawnTunables(String world, int[] values);

    /** The view distance after setting it to a positive value; a zero only reads it. */
    public static native int viewDistance(String world, int set);

    /** Sea level, then the ticks left of rain, thunder and commanded clear weather. */
    public static native int[] worldFacts(String world);

    public static native void setWeatherTimes(String world, int rain, int thunder, int clear);

    /** The force-loaded chunks as x and z pairs. */
    public static native int[] forcedChunks(String world);

    public static native void setChunkForced(String world, int chunkX, int chunkZ, boolean forced);

    /** The inhabited ticks of a loaded chunk, after setting them when `set` is not negative. */
    public static native long inhabitedTime(String world, int chunkX, int chunkZ, long set);

    /** Id, `boolean` or `int`, and starting value of every game rule. */
    public static native String[] gameRuleSpecs();

    public static native int gameEventFrequency(String event);

    /** Tells the sender of the plugin command running now, which is not the console or a player. */
    public static native void commandReply(String miniMessage);

    /** Has the chunks of a void world made by `CustomGenerators.fill`; false when the world is not a void world. */
    public static native boolean useGenerator(String world);

    /** One server setting as text, null when there is none of that name. */
    public static native String setting(String key);

    public static native boolean setSetting(String key, String value);

    /** Makes a world of the dimension (overworld, nether, end) and type; true when it exists afterwards. */
    public static native boolean createWorld(String name, String dimension, String type, long seed, boolean seeded);

    public static native boolean unloadWorld(String name, boolean save);

    public static native boolean stopping();

    public static native int transformState(int state, int rotation, int mirror);

    /** Whether the tool is right for the block's drops, then how fast it mines it. */
    public static native double[] toolFacts(String item, int state);
}
