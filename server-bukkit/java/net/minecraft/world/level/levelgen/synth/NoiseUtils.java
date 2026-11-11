// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.synth;

import java.util.Locale;

public class NoiseUtils {
    public static double biasTowardsExtreme(double noise, double factor) {
        return noise + Math.sin(Math.PI * noise) * factor / Math.PI;
    }

    public static void parityNoiseOctaveConfigString(StringBuilder sb, double xo, double yo, double zo, byte[] p) {
        describe(sb, xo, yo, zo, p[0], p[255]);
    }

    public static void parityNoiseOctaveConfigString(StringBuilder sb, double xo, double yo, double zo, int[] p) {
        describe(sb, xo, yo, zo, p[0], p[255]);
    }

    private static void describe(StringBuilder sb, double xo, double yo, double zo, int first, int last) {
        sb.append(String.format(Locale.ROOT, "xo=%.3f, yo=%.3f, zo=%.3f, p0=%d, p255=%d", (float) xo, (float) yo,
            (float) zo, first, last));
    }
}
