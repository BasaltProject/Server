package org.basaltmc.bukkit;

public final class LeftoverNatives {
    private LeftoverNatives() {}

    public static native String[] structureIds();
    public static native String[] stopwatchIds();
    public static native double stopwatch(String operation, String id);
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
    public static native boolean structurePlace(byte[] nbt, String world, int x, int y, int z, int rotation,
        int mirror, float integrity, long seed, boolean entities, int palette);
    public static native byte[] registryData(String registry, String id);
    public static native String[] entityDefaults(String type);
    public static native double humidity(String world, int x, int y, int z);
    public static native boolean dragonAction(String world, String action, int value);

    public static native boolean dragonBar(String world, String action, String text, int value);
    public static native boolean windowProperty(String uuid, int id, int value);
    public static native String[] enchantItem(String[] stack, String world, String options, int levels, long seed);
    public static native byte[] explorerMap(String world, double x, double y, double z, String[] destination,
        String decoration, int zoom, int radius, boolean skipExisting);
}
