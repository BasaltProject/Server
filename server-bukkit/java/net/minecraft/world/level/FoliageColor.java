// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

public class FoliageColor {
    public static final int FOLIAGE_EVERGREEN = -10380959;
    public static final int FOLIAGE_BIRCH = -8345771;
    public static final int FOLIAGE_DEFAULT = -12012264;
    public static final int FOLIAGE_MANGROVE = -7158200;
    private static int[] kMap = new int[65536];
    public static void init(int[] pixels) {
        kMap = pixels;
    }
    public static int get(double temp, double rain) {
        return ColorMapColorUtil.get(temp, rain, kMap, FOLIAGE_DEFAULT);
    }
}
