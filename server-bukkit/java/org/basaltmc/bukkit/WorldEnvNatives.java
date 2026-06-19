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

    public static native int transformState(int state, int rotation, int mirror);

    /** Whether the tool is right for the block's drops, then how fast it mines it. */
    public static native double[] toolFacts(String item, int state);
}
