package org.basaltmc.bukkit;

public final class WorldNatives {
    private WorldNatives() {}

    public static native int stateId(String text);
    public static native String stateText(int id);
    public static native int stateFlags(int id);
    public static native String[] blockTable();
    public static native String[] blockProperties(String block);
    public static native double[] blockHardness(String block);
    public static native double[] blockInfo(int state);
    public static native String[] biomeIds();
    public static native String[] itemIds();
    public static native String[] tags(String registry);
    public static native String[] entityTypes();

    public static native String[] worldStrings(String world);
    public static native long[] worldNumbers(String world);
    public static native void setTime(String world, long ticks);
    public static native void setWeather(String world, boolean raining, boolean thundering);
    public static native void setSpawn(String world, int x, int y, int z);
    public static native boolean setDifficulty(String world, String difficulty);
    public static native String gameRule(String world, String rule);
    public static native boolean setGameRule(String world, String rule, String value);
    public static native String[] gameRules(String world);
    public static native void saveWorld(String world);

    public static native int state(String world, int x, int y, int z, boolean load);
    public static native boolean setState(String world, int x, int y, int z, int state, boolean notify);
    public static native int readStates(String world, int[] positions, int[] out, boolean load);
    public static native int writeStates(String world, int[] positions, int[] states, boolean notify);
    public static native boolean readSection(String world, int chunkX, int sectionY, int chunkZ, boolean load, int[] out);
    public static native boolean writeSection(String world, int chunkX, int sectionY, int chunkZ, int[] states, boolean notify);
    public static native int applySections(String world, int chunkX, int chunkZ, int[] sections, int[] states, boolean resend, int[] previous);
    public static native int worldHandle(String world);
    public static native long blockVersion(int handle);
    public static native int applyChunks(String world, int[] chunks, int[] sections, int[] states, boolean resend, int[] previous);
    public static native byte[] blockEntityNbt(String world, int x, int y, int z);
    public static native boolean setBlockEntityNbt(String world, int x, int y, int z, byte[] nbt);
    public static native int[] blockEntityPositions(String world, int chunkX, int chunkZ);
    public static native byte[] entityNbt(String world, int id);
    public static native int spawnEntityNbt(String world, byte[] nbt);
    public static native boolean readBiomes(String world, int chunkX, int chunkZ, int[] out);
    public static native boolean writeBiomes(String world, int chunkX, int chunkZ, int[] ids);
    public static native void relightChunks(String world, int[] chunks);
    public static native boolean readSections(String world, int chunkX, int chunkZ, boolean load, int[] out);
    public static native int chunkStatus(String world, int chunkX, int chunkZ);
    public static native void requestChunk(String world, int chunkX, int chunkZ, boolean generate, int priority, java.util.concurrent.CompletableFuture<Boolean> done);
    public static native boolean holdChunks(String world, String token, int[] chunks);
    public static native void releaseChunks(String world, String token);
    public static native int updateNeighbors(String world, int[] positions, int effects);
    public static native int applyChanges(String world, int[] positions, int[] states, int effects);
    public static native void resendChunks(String world, int[] chunks);
    public static native void lightJob(String world, int[] chunks, java.util.concurrent.CompletableFuture<Integer> done);
    public static native int lightAt(String world, int x, int y, int z, int layer);
    public static native byte[] readLight(String world, int chunkX, int sectionY, int chunkZ, int layer, boolean load);
    public static native boolean writeLight(String world, int chunkX, int sectionY, int chunkZ, int layer, byte[] nibbles);
    public static native void checkLight(String world, int[] positions);
    public static native String biome(String world, int x, int y, int z);
    public static native boolean setBiome(String world, int x, int y, int z, String biome);
    public static native int highest(String world, int x, int z, String kind, boolean load);

    public static native boolean chunkLoaded(String world, int chunkX, int chunkZ);
    public static native boolean loadChunk(String world, int chunkX, int chunkZ, boolean generate);
    public static native boolean unloadChunk(String world, int chunkX, int chunkZ);
    public static native int useItemOn(String world, int x, int y, int z, int face, String item, float yaw, float pitch);
    public static native boolean scheduleTick(String world, int x, int y, int z, int delay, boolean fluid);
    public static native int settleBlocks(String world, int[] positions, int[] previous, int effects);
    public static native int neighborChanged(String world, int[] positions, int[] sources);
    public static native int updateShapes(String world, int[] positions, int[] states, boolean indirect);
    public static native int[] loadedChunks(String world);

    public static native int[] entityIds(String world, int chunkX, int chunkZ, boolean all);
    public static native String[] entityStrings(String world, int id);
    public static native double[] entityNumbers(String world, int id);
    public static native boolean entityMove(String world, int id, double x, double y, double z, float yaw, float pitch);
    public static native void entityVelocity(String world, int id, double x, double y, double z);
    public static native void entityHealth(String world, int id, double health);
    public static native void entityRemove(String world, int id);
    public static native int spawn(String world, String type, double x, double y, double z);

    public static native boolean playerMove(String uuid, String world, double x, double y, double z, float yaw, float pitch);
    public static native double[] playerExtra(String uuid);
    public static native String playerAddress(String uuid);
    public static native void playerShowBlock(String uuid, int x, int y, int z, int state);
    public static native void playerExperience(String uuid, float progress, int level);
    public static native boolean playerCommand(String uuid, String line);
    public static native void playerTitle(String uuid, String title, String subtitle, int fadeIn, int stay, int fadeOut);
    public static native void playerSound(String uuid, String sound, float volume, float pitch);
    public static native int inventorySize(String uuid);
    public static native int inventoryHeld(String uuid);
    public static native String[] inventoryGet(String uuid, int slot);
    public static native void inventorySet(String uuid, int slot, String[] item);
    public static native void inventoryClear(String uuid);
}
