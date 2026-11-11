// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

public class GrassColor {
    private static int[] kMap = new int[65536];
    public static void init(int[] pixels) {
        kMap = pixels;
    }
    public static int get(double temp, double rain) {
        return ColorMapColorUtil.get(temp, rain, kMap, -65281);
    }
    public static int getDefaultColor() {
        return get(0.5, 1.0);
    }
}
