package org.basaltmc.bukkit;

public final class RestNatives {
    private RestNatives() {}

    public static native long[] tickTimes();

    public static native boolean paused();

    /** NaN when the world has no such tunable. */
    public static native double worldTunable(String world, String key);

    public static native boolean setWorldTunable(String world, String key, double value);

    public static native boolean randomTick(String world, int x, int y, int z);

    public static native int tickableBlockEntities(String world);

    /** Five numbers per block (x, y, z, state before, state after); null when the tree does not fit. */
    public static native int[] placeTree(String world, String tree, int x, int y, int z, boolean apply);

    public static native String blockInstrument(int state);

    public static native byte[] serverIcon();

    /** Hands the world chunks to the plugin generator; flags: 1 vanilla terrain, 2 decorations, 4 structures, 8 mobs. */
    public static native boolean useGeneratorWith(String world, int flags);

    public static native String poiOfState(int state);

    public static native int poiTickets(String type);

    public static native long offlineStatistic(String uuid, String key);

    public static native boolean setOfflineStatistic(String uuid, String key, long value);

    /** The dimension key, x, y and z of the last death; null when the player never died. */
    public static native String[] lastDeath(String uuid);

    public static native int nextEntityId();

    public static native boolean reloadData();

    public static native boolean placeEndGateway(String world, int x, int y, int z);

    public static native boolean reloadCommands();
}
