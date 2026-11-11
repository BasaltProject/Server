// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.jfr;
import com.google.common.math.Quantiles;
import java.util.Map;
public class Percentiles {
    public static final Quantiles.ScaleAndIndexes DEFAULT_INDEXES = Quantiles.scale(100).indexes(50, 75, 90, 99);
    private Percentiles() {
        
    }
    public static Map<Integer, Double> evaluate(long[] dataset) {
        return java.util.Map.of();
    }
    public static Map<Integer, Double> evaluate(int[] dataset) {
        return java.util.Map.of();
    }
    public static Map<Integer, Double> evaluate(double[] dataset) {
        return java.util.Map.of();
    }
}
