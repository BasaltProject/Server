package org.basaltmc.bukkit;

public final class PlayerNatives {
    private PlayerNatives() {}

    public static native Object get(String uuid, String name);
    public static native boolean set(String uuid, String name, Object value);
    public static native Object call(String uuid, String operation, Object[] arguments);
}
