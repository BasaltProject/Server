// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

public interface ColorMapColorUtil {
    static int get(double temp, double rain, int[] pixels, int defaultMapColor) {
        int column = (int) ((1.0 - temp) * 255.0);
        int row = (int) ((1.0 - rain * temp) * 255.0);
        int at = row << 8 | column;
        return at >= pixels.length ? defaultMapColor : pixels[at];
    }
}
