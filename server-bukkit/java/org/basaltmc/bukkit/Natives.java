package org.basaltmc.bukkit;

public final class Natives {
    private Natives() {}

    public static native void log(int level, String logger, String message);
    public static native long currentTick();
    public static native void scheduleAfter(long id, int delay);
    public static native void scheduleEvery(long id, int delay, int period);
    public static native void cancelTask(long id);
    public static native boolean subscribe(String event, int priority);
    public static native String[] eventFields(String event);
    public static native void registerCommand(String plugin, String name, String permission);
    public static native void unregisterCommands(String plugin);
    public static native boolean runConsoleCommand(String line);
    public static native String pluginFolder(String id);
    public static native String[] onlinePlayers();
    public static native String[] playerStrings(String uuid);
    public static native double[] playerNumbers(String uuid);
    public static native void playerSend(String uuid, String miniMessage);
    public static native void playerActionBar(String uuid, String miniMessage);
    public static native void playerKick(String uuid, String reason);
    public static native boolean playerTeleport(String uuid, String world, double x, double y, double z);
    public static native boolean playerMay(String uuid, String permission);
    public static native void playerSetHealth(String uuid, double value);
    public static native void playerSetFood(String uuid, int value);
    public static native void playerSetGamemode(String uuid, String mode);
    public static native boolean playerGive(String uuid, String item, int count);
    public static native String[] worlds();
    public static native String blockAt(String world, int x, int y, int z);
    public static native boolean setBlockAt(String world, int x, int y, int z, String block);
    public static native long worldTime(String world);
}
