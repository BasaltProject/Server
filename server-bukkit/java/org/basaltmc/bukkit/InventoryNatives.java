package org.basaltmc.bukkit;

public final class InventoryNatives {
    private InventoryNatives() {}

    public static native byte[] playerItem(String uuid, int slot);
    public static native void setPlayerItem(String uuid, int slot, byte[] item);
    public static native int windowOpen(String uuid, String kind, int size, byte[] title, byte[][] items, boolean editable);
    public static native void windowClose(String uuid);
    public static native String[] windowInfo(String uuid);
    public static native byte[] windowItem(String uuid, int slot);
    public static native void setWindowItem(String uuid, int slot, byte[] item);
    public static native byte[] cursorItem(String uuid);
    public static native void setCursorItem(String uuid, byte[] item);
    public static native void windowResync(String uuid);
    public static native void setHeldSlot(String uuid, int slot);
    public static native byte[] itemData(String id);
    public static native byte[] enchantmentData(String id);
    public static native byte[] parseSnbt(String text);
    public static native String[] registryTags(String registry);
    public static native String[] enchantmentIds();
    public static native byte[] craft(int width, int height, byte[] items);
    public static native String[] recipeIds();
    public static native byte[] recipeData(String id);
    public static native boolean addRecipe(String id, byte[] data);
    public static native boolean removeRecipe(String id);
    public static native void clearRecipes();
    public static native void resetRecipes();
    public static native byte[] mobEffects();
    public static native byte[] potions();
}
