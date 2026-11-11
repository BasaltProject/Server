// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

public class DryFoliageColor {
    public static final int FOLIAGE_DRY_DEFAULT = -10732494;
    private static int[] kMap = new int[65536];
    public static void init(int[] pixels) {
        kMap = pixels;
    }
    public static int get(double temp, double rain) {
        return ColorMapColorUtil.get(temp, rain, kMap, FOLIAGE_DRY_DEFAULT);
    }
}
