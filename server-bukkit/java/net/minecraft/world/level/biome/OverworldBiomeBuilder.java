// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;
import com.mojang.datafixers.util.Pair;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.OverworldFunctionSet;
import net.minecraft.world.level.levelgen.SpawnTargetPoint;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.NoiseRouterData;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunctions;
import net.minecraft.resources.Identifier;
import net.minecraft.core.registries.Registries;
import java.util.Map;
import org.basaltmc.bukkit.WorldEnvNatives;
import java.util.ArrayList;
public final class OverworldBiomeBuilder {
    public static final float HIGH_START = 0.4F;
    public static final float PEAK_START = 0.56666666F;
    public static final float NEAR_INLAND_START = -0.11F;
    public static final float MID_INLAND_START = 0.03F;
    public static final float FAR_INLAND_START = 0.3F;
    public static final float EROSION_INDEX_1_START = -0.78F;
    public static final float EROSION_INDEX_2_START = -0.375F;
    private static final float kLowStart = 0.26666668F;
    private final Climate.Parameter fullRange = Climate.Parameter.span(-1.0F, 1.0F);
    private final Climate.Parameter[] temperatures = spans(-1.0F, -0.45F, -0.15F, 0.2F, 0.55F, 1.0F);
    private final Climate.Parameter[] humidities = spans(-1.0F, -0.35F, -0.1F, 0.1F, 0.3F, 1.0F);
    private final Climate.Parameter[] erosions = spans(-1.0F, -0.78F, -0.375F, -0.2225F, 0.05F, 0.45F, 0.55F, 1.0F);
    private final Climate.Parameter mushroomFieldsContinentalness = Climate.Parameter.span(-1.2F, -1.05F);
    private final Climate.Parameter deepOceanContinentalness = Climate.Parameter.span(-1.05F, -0.455F);
    private final Climate.Parameter oceanContinentalness = Climate.Parameter.span(-0.455F, -0.19F);
    private final Climate.Parameter coastContinentalness = Climate.Parameter.span(-0.19F, -0.11F);
    private final Climate.Parameter inlandContinentalness = Climate.Parameter.span(-0.11F, 0.55F);
    private final Climate.Parameter nearInlandContinentalness = Climate.Parameter.span(-0.11F, 0.03F);
    private final Climate.Parameter midInlandContinentalness = Climate.Parameter.span(0.03F, 0.3F);
    private final Climate.Parameter farInlandContinentalness = Climate.Parameter.span(0.3F, 1.0F);
    public OverworldBiomeBuilder() {
    }
    private static Climate.Parameter[] spans(float... edges) {
        Climate.Parameter[] out = new Climate.Parameter[edges.length - 1];
        for (int at = 0; at < out.length; at++) {
            out[at] = Climate.Parameter.span(edges[at], edges[at + 1]);
        }
        return out;
    }
    public List<SpawnTargetPoint> spawnTarget(OverworldFunctionSet<Holder<DensityFunction>> functions, Holder<DensityFunction> weirdness) {
        Climate.Parameter inland = Climate.Parameter.span(inlandContinentalness, fullRange);
        List<SpawnTargetPoint> targets = new java.util.ArrayList<>();
        for (Climate.Parameter side : new Climate.Parameter[] {Climate.Parameter.span(-1.0F, -0.16F), Climate.Parameter.span(0.16F, 1.0F)}) {
            targets.add(new SpawnTargetPoint(Map.of(functions.temperature(), fullRange, functions.vegetation(), fullRange,
                functions.continents(), inland, functions.erosion(), fullRange, weirdness, side)));
        }
        return List.copyOf(targets);
    }
    void addBiomes(Consumer<Pair<Climate.ParameterPoint, ResourceKey<Biome>>> biomes) {
        long[] flat = WorldEnvNatives.overworldBiomePoints();
        String[] names = WorldEnvNatives.overworldBiomeNames();
        for (int at = 0; at < names.length; at++) {
            int base = at * 13;
            Climate.Parameter[] axes = new Climate.Parameter[6];
            for (int axis = 0; axis < 6; axis++) {
                axes[axis] = new Climate.Parameter(flat[base + axis * 2], flat[base + axis * 2 + 1]);
            }
            Climate.ParameterPoint point = new Climate.ParameterPoint(axes[0], axes[1], axes[2], axes[3], axes[4], axes[5], flat[base + 12]);
            biomes.accept(Pair.of(point, ResourceKey.create(Registries.BIOME, Identifier.parse(names[at]))));
        }
    }
    public static DensityFunction deepDarkRegion(DensityFunction erosion, DensityFunction depth) {
        DensityFunction dryness = DensityFunctions.sub(DensityFunctions.constant(-0.225F), erosion);
        return DensityFunctions.min(dryness, DensityFunctions.max(depth.sub(0.9F), DensityFunctions.constant(0.0F)));
    }
    public static String getDebugStringForPeaksAndValleys(double peaksAndValleys) {
        if (peaksAndValleys < NoiseRouterData.peaksAndValleys(0.05F)) {
            return "Valley";
        }
        if (peaksAndValleys < NoiseRouterData.peaksAndValleys(kLowStart)) {
            return "Low";
        }
        if (peaksAndValleys < NoiseRouterData.peaksAndValleys(HIGH_START)) {
            return "Mid";
        }
        return peaksAndValleys < NoiseRouterData.peaksAndValleys(PEAK_START) ? "High" : "Peak";
    }
    public String getDebugStringForContinentalness(double continentalness) {
        double q = Climate.quantizeCoord((float) continentalness);
        if (q < mushroomFieldsContinentalness.max()) {
            return "Mushroom fields";
        }
        if (q < deepOceanContinentalness.max()) {
            return "Deep ocean";
        }
        if (q < oceanContinentalness.max()) {
            return "Ocean";
        }
        if (q < coastContinentalness.max()) {
            return "Coast";
        }
        if (q < nearInlandContinentalness.max()) {
            return "Near inland";
        }
        return q < midInlandContinentalness.max() ? "Mid inland" : "Far inland";
    }
    public String getDebugStringForErosion(double erosion) {
        return bucket(erosion, erosions);
    }
    public String getDebugStringForTemperature(double temperature) {
        return bucket(temperature, temperatures);
    }
    public String getDebugStringForHumidity(double humidity) {
        return bucket(humidity, humidities);
    }
    private static String bucket(double value, Climate.Parameter[] ranges) {
        double q = Climate.quantizeCoord((float) value);
        for (int at = 0; at < ranges.length; at++) {
            if (q < ranges[at].max()) {
                return Integer.toString(at);
            }
        }
        return "?";
    }
    public Climate.Parameter[] getTemperatureThresholds() {
        return temperatures;
    }
    public Climate.Parameter[] getHumidityThresholds() {
        return humidities;
    }
    public Climate.Parameter[] getErosionThresholds() {
        return erosions;
    }
    public Climate.Parameter[] getContinentalnessThresholds() {
        return new Climate.Parameter[] {mushroomFieldsContinentalness, deepOceanContinentalness, oceanContinentalness, coastContinentalness,
            nearInlandContinentalness, midInlandContinentalness, farInlandContinentalness};
    }
    public Climate.Parameter[] getPeaksAndValleysThresholds() {
        float[] marks = {NoiseRouterData.peaksAndValleys(0.05F), NoiseRouterData.peaksAndValleys(kLowStart), NoiseRouterData.peaksAndValleys(HIGH_START),
            NoiseRouterData.peaksAndValleys(PEAK_START), 2.0F};
        Climate.Parameter[] out = new Climate.Parameter[marks.length];
        float from = -2.0F;
        for (int at = 0; at < marks.length; at++) {
            out[at] = Climate.Parameter.span(from, marks[at]);
            from = marks[at];
        }
        return out;
    }
    public Climate.Parameter[] getWeirdnessThresholds() {
        return new Climate.Parameter[] {Climate.Parameter.span(-2.0F, 0.0F), Climate.Parameter.span(0.0F, 2.0F)};
    }
}
