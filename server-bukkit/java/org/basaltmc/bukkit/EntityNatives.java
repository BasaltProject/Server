package org.basaltmc.bukkit;

public final class EntityNatives {
    private EntityNatives() {}

    public static native Object get(String world, int id, String name);
    public static native boolean set(String world, int id, String name, Object value);
    public static native Object call(String world, int id, String operation, Object[] arguments);
    public static native int dropItem(String world, byte[] stack, double x, double y, double z, double vx, double vy,
        double vz);
    public static native boolean explode(String world, double x, double y, double z, float power, boolean breakBlocks,
        int source, boolean fire);
    public static native byte[] snbtToNbt(String snbt);
    public static native String nbtToSnbt(byte[] nbt);
    public static native int spawnAs(String world, String type, double x, double y, double z, float yaw, String reason);
}
