// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import it.unimi.dsi.fastutil.floats.FloatList;
import java.util.List;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.core.Vec3i;
import net.minecraft.util.CubicSpline;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.op.RoundFunction;
import net.minecraft.world.level.levelgen.densityfunction.op.SplineFunction;
import net.minecraft.world.level.levelgen.synth.NormalNoise;
public final class DensityFunctions {
    public static final float MAX_REASONABLE_NOISE_VALUE = 1000000.0F;
    public static final Codec<Float> NOISE_VALUE_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.densityfunction.DensityFunctions.class, "NOISE_VALUE_CODEC");
    public static final Codec<DensityFunction> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.densityfunction.DensityFunctions.class, "DIRECT_CODEC");
    public static MapCodec<? extends DensityFunction> bootstrap(Registry<MapCodec<? extends DensityFunction>> registry) {
        MapCodec<? extends DensityFunction> constant = enter(registry, "constant", net.minecraft.world.level.levelgen.densityfunction.generator.ConstantFunction.CODEC);
        for (net.minecraft.world.level.levelgen.densityfunction.generator.SimpleDensityFunction simple : net.minecraft.world.level.levelgen.densityfunction.generator.SimpleDensityFunction.values()) {
            enter(registry, simple.id(), simple.codec());
        }
        enter(registry, "noise", net.minecraft.world.level.levelgen.densityfunction.generator.NoiseFunction.CODEC);
        enter(registry, "end_outer_islands", net.minecraft.world.level.levelgen.densityfunction.generator.EndIslandFunction.CODEC);
        enter(registry, "distance_to_point", net.minecraft.world.level.levelgen.densityfunction.generator.DistanceToPointFunction.CODEC);
        enter(registry, "gradient", net.minecraft.world.level.levelgen.densityfunction.generator.GradientFunction.CODEC);
        enter(registry, "shift_a", net.minecraft.world.level.levelgen.densityfunction.generator.ShiftNoiseFunction.ShiftA.CODEC);
        enter(registry, "shift_b", net.minecraft.world.level.levelgen.densityfunction.generator.ShiftNoiseFunction.ShiftB.CODEC);
        enter(registry, "shift", net.minecraft.world.level.levelgen.densityfunction.generator.ShiftNoiseFunction.Shift.CODEC);
        for (net.minecraft.world.level.levelgen.densityfunction.op.UnaryFunction.Type type : net.minecraft.world.level.levelgen.densityfunction.op.UnaryFunction.Type.values()) {
            enter(registry, type.id, type.codec);
        }
        for (RoundFunction.Type type : RoundFunction.Type.values()) {
            enter(registry, type.id, type.codec);
        }
        for (net.minecraft.world.level.levelgen.densityfunction.op.BinaryFunction.Type type : net.minecraft.world.level.levelgen.densityfunction.op.BinaryFunction.Type.values()) {
            enter(registry, type.id, type.codec);
        }
        enter(registry, "pow", net.minecraft.world.level.levelgen.densityfunction.op.PowFunction.CODEC);
        enter(registry, "spline", SplineFunction.CODEC);
        enter(registry, "lerp", net.minecraft.world.level.levelgen.densityfunction.op.LerpFunction.CODEC);
        enter(registry, "clamp", net.minecraft.world.level.levelgen.densityfunction.op.ClampFunction.CODEC);
        enter(registry, "range_choice", net.minecraft.world.level.levelgen.densityfunction.op.RangeChoiceFunction.CODEC);
        enter(registry, "interval_select", net.minecraft.world.level.levelgen.densityfunction.op.IntervalSelectFunction.CODEC);
        enter(registry, "cache", net.minecraft.world.level.levelgen.densityfunction.op.CacheFunction.CODEC);
        enter(registry, "blend_density", net.minecraft.world.level.levelgen.densityfunction.op.BlendDensityFunction.CODEC);
        enter(registry, "interpolated", net.minecraft.world.level.levelgen.densityfunction.op.InterpolatedFunction.CODEC);
        enter(registry, "slice", net.minecraft.world.level.levelgen.densityfunction.op.SliceFunction.CODEC);
        enter(registry, "find_top_surface", net.minecraft.world.level.levelgen.densityfunction.op.FindTopSurfaceFunction.CODEC);
        enter(registry, "old_blended_noise", net.minecraft.world.level.levelgen.synth.BlendedNoise.CODEC);
        return constant;
    }
    private static MapCodec<? extends DensityFunction> enter(Registry<MapCodec<? extends DensityFunction>> registry, String name, MapCodec<? extends DensityFunction> codec) {
        return Registry.register(registry, name, codec);
    }
    private DensityFunctions() {
        
    }
    public static DensityFunction zero() {
        return constant(0.0F);
    }
    public static DensityFunction constant(float value) {
        return JsonDensityFunction.constant(value);
    }
    public static DensityFunction abs(DensityFunction input) {
        return JsonDensityFunction.node("minecraft:abs", "input", input);
    }
    public static DensityFunction square(DensityFunction input) {
        return JsonDensityFunction.node("minecraft:square", "input", input);
    }
    public static DensityFunction cube(DensityFunction input) {
        return JsonDensityFunction.node("minecraft:cube", "input", input);
    }
    public static DensityFunction sqrt(DensityFunction input) {
        return JsonDensityFunction.node("minecraft:sqrt", "input", input);
    }
    public static DensityFunction halfNegative(DensityFunction input) {
        return JsonDensityFunction.node("minecraft:half_negative", "input", input);
    }
    public static DensityFunction quarterNegative(DensityFunction input) {
        return JsonDensityFunction.node("minecraft:quarter_negative", "input", input);
    }
    public static DensityFunction reciprocal(DensityFunction input) {
        return JsonDensityFunction.node("minecraft:reciprocal", "input", input);
    }
    public static DensityFunction negate(DensityFunction input) {
        return JsonDensityFunction.node("minecraft:negate", "input", input);
    }
    public static DensityFunction squeeze(DensityFunction input) {
        return JsonDensityFunction.node("minecraft:squeeze", "input", input);
    }
    public static DensityFunction log(DensityFunction input) {
        return JsonDensityFunction.node("minecraft:log", "input", input);
    }
    public static DensityFunction sign(DensityFunction input) {
        return JsonDensityFunction.node("minecraft:sign", "input", input);
    }
    public static DensityFunction interpolated(DensityFunction function, int cellSizeXz, int cellSizeY) {
        return JsonDensityFunction.raw("minecraft:interpolated", function, "\"cell_size_xz\":" + cellSizeXz + ",\"cell_size_y\":" + cellSizeY);
    }
    public static DensityFunction cache(DensityFunction function) {
        return JsonDensityFunction.node("minecraft:cache", "input", function);
    }
    public static DensityFunction mappedNoise(Holder<NormalNoise> noiseData, double xzScale, double yScale, float minTarget, float maxTarget) {
        return add(constant(minTarget), mul(add(mul(noise(noiseData, xzScale, yScale), constant(0.5F)), constant(0.5F)), constant(maxTarget - minTarget)));
    }
    public static DensityFunction mappedNoise(Holder<NormalNoise> noiseData, double yScale, float minTarget, float maxTarget) {
        return mappedNoise(noiseData, 1.0, yScale, minTarget, maxTarget);
    }
    public static DensityFunction mappedNoise(Holder<NormalNoise> noiseData, float minTarget, float maxTarget) {
        return mappedNoise(noiseData, 1.0, 1.0, minTarget, maxTarget);
    }
    public static DensityFunction shiftedNoise2d(DensityFunction shiftX, DensityFunction shiftZ, double xzScale, Holder<NormalNoise> noiseData) {
        return JsonDensityFunction.raw("minecraft:noise", null, "\"noise\":" + JsonDensityFunction.key(noiseData) + ",\"xz_scale\":" + xzScale + ",\"y_scale\":0.0,\"shift_x\":" + JsonDensityFunction.of(shiftX) + ",\"shift_z\":" + JsonDensityFunction.of(shiftZ));
    }
    public static DensityFunction noise(Holder<NormalNoise> noiseData) {
        return noise(noiseData, 1.0, 1.0);
    }
    public static DensityFunction noise(Holder<NormalNoise> noiseData, double xzScale, double yScale) {
        return JsonDensityFunction.raw("minecraft:noise", null, "\"noise\":" + JsonDensityFunction.key(noiseData) + ",\"xz_scale\":" + xzScale + ",\"y_scale\":" + yScale);
    }
    public static DensityFunction noise(Holder<NormalNoise> noiseData, double yScale) {
        return noise(noiseData, 1.0, yScale);
    }
    public static DensityFunction rangeChoice(DensityFunction input, float minInclusive, float maxExclusive, DensityFunction whenInRange, DensityFunction whenOutOfRange) {
        return JsonDensityFunction.raw("minecraft:range_choice", input, "\"min_inclusive\":" + minInclusive + ",\"max_exclusive\":" + maxExclusive + ",\"when_in_range\":" + JsonDensityFunction.of(whenInRange) + ",\"when_out_of_range\":" + JsonDensityFunction.of(whenOutOfRange));
    }
    public static DensityFunction intervalSelect(DensityFunction input, FloatList thresholds, List<DensityFunction> functions) {
        return JsonDensityFunction.intervalSelect(input, thresholds, functions);
    }
    public static DensityFunction shiftA(Holder<NormalNoise> noiseData) {
        return JsonDensityFunction.raw("minecraft:shift_a", null, "\"noise\":" + JsonDensityFunction.key(noiseData));
    }
    public static DensityFunction shiftB(Holder<NormalNoise> noiseData) {
        return JsonDensityFunction.raw("minecraft:shift_b", null, "\"noise\":" + JsonDensityFunction.key(noiseData));
    }
    public static DensityFunction shift(Holder<NormalNoise> noiseData) {
        return JsonDensityFunction.raw("minecraft:shift", null, "\"noise\":" + JsonDensityFunction.key(noiseData));
    }
    public static DensityFunction blendDensity(DensityFunction input) {
        return JsonDensityFunction.node("minecraft:blend_density", "input", input);
    }
    public static DensityFunction endOuterIslands() {
        return JsonDensityFunction.raw("minecraft:end_outer_islands", null, null);
    }
    public static DensityFunction distanceToPoint(Vec3i point, DistanceMetric metric) {
        return JsonDensityFunction.raw("minecraft:distance_to_point", null, "\"point\":[" + point.getX() + "," + point.getY() + "," + point.getZ() + "],\"metric\":\"" + metric.getSerializedName() + "\"");
    }
    public static DensityFunction add(DensityFunction left, DensityFunction right) {
        return JsonDensityFunction.binary("minecraft:add", left, right);
    }
    public static DensityFunction sub(DensityFunction left, DensityFunction right) {
        return JsonDensityFunction.binary("minecraft:sub", left, right);
    }
    public static DensityFunction mul(DensityFunction left, DensityFunction right) {
        return JsonDensityFunction.binary("minecraft:mul", left, right);
    }
    public static DensityFunction div(DensityFunction left, DensityFunction right) {
        return JsonDensityFunction.binary("minecraft:div", left, right);
    }
    public static DensityFunction pow(DensityFunction base, DensityFunction exponent) {
        return JsonDensityFunction.binary("minecraft:pow", base, exponent);
    }
    public static DensityFunction clamp(DensityFunction input, float min, float max) {
        return JsonDensityFunction.raw("minecraft:clamp", input, "\"min\":" + min + ",\"max\":" + max);
    }
    public static DensityFunction min(DensityFunction left, DensityFunction right) {
        return JsonDensityFunction.binary("minecraft:min", left, right);
    }
    public static DensityFunction max(DensityFunction left, DensityFunction right) {
        return JsonDensityFunction.binary("minecraft:max", left, right);
    }
    public static DensityFunction spline(CubicSpline<SplineFunction.Coordinate> spline) {
        return JsonDensityFunction.wrap(new SplineFunction(spline).treeJson());
    }
    public static DensityFunction sliceY(DensityFunction input, int y) {
        return slice(Direction.Axis.Y, y, input);
    }
    public static DensityFunction slice(Direction.Axis axis, int coordinate, DensityFunction input) {
        return JsonDensityFunction.raw("minecraft:slice", input, "\"axis\":\"" + axis.getSerializedName() + "\",\"coordinate\":" + coordinate);
    }
    public static DensityFunction yClampedGradient(int fromY, int toY, float fromValue, float toValue) {
        return gradient(Direction.Axis.Y, TilingMode.CLAMP_TO_EDGE, fromY, toY, fromValue, toValue);
    }
    public static DensityFunction gradient(Direction.Axis axis, TilingMode tiling, int fromCoordinate, int toCoordinate, float fromValue, float toValue) {
        return JsonDensityFunction.raw("minecraft:gradient", null, "\"axis\":\"" + axis.getSerializedName() + "\",\"tiling\":\"" + tiling.getSerializedName() + "\",\"from_coordinate\":" + fromCoordinate + ",\"to_coordinate\":" + toCoordinate + ",\"from_value\":" + fromValue + ",\"to_value\":" + toValue);
    }
    public static DensityFunction blendAlpha() {
        return JsonDensityFunction.raw("minecraft:blend_alpha", null, null);
    }
    public static DensityFunction blendOffset() {
        return JsonDensityFunction.raw("minecraft:blend_offset", null, null);
    }
    public static DensityFunction beardifier() {
        return JsonDensityFunction.raw("minecraft:beardifier", null, null);
    }
    public static DensityFunction lerp(DensityFunction alpha, DensityFunction first, DensityFunction second) {
        return JsonDensityFunction.raw("minecraft:lerp", null, "\"alpha\":" + JsonDensityFunction.of(alpha) + ",\"first\":" + JsonDensityFunction.of(first) + ",\"second\":" + JsonDensityFunction.of(second));
    }
    public static DensityFunction lerp(DensityFunction factor, float first, DensityFunction second) {
        return lerp(factor, constant(first), second);
    }
    public static DensityFunction round(RoundFunction.Type type, DensityFunction input, DensityFunction multiple) {
        return JsonDensityFunction.raw("minecraft:" + type.id, input, "\"multiple\":" + JsonDensityFunction.of(multiple));
    }
    public static DensityFunction round(RoundFunction.Type type, DensityFunction input) {
        return DensityFunctions.round(type, input, constant(1.0F));
    }
    public static DensityFunction floor(DensityFunction input, DensityFunction multiple) {
        return DensityFunctions.round(RoundFunction.Type.FLOOR, input, multiple);
    }
    public static DensityFunction floor(DensityFunction input) {
        return round(RoundFunction.Type.FLOOR, input);
    }
    public static DensityFunction round(DensityFunction input, DensityFunction multiple) {
        return DensityFunctions.round(RoundFunction.Type.ROUND, input, multiple);
    }
    public static DensityFunction round(DensityFunction input) {
        return round(RoundFunction.Type.ROUND, input);
    }
    public static DensityFunction ceil(DensityFunction input, DensityFunction multiple) {
        return DensityFunctions.round(RoundFunction.Type.CEIL, input, multiple);
    }
    public static DensityFunction ceil(DensityFunction input) {
        return round(RoundFunction.Type.CEIL, input);
    }
    public static DensityFunction truncate(DensityFunction input, DensityFunction multiple) {
        return DensityFunctions.round(RoundFunction.Type.TRUNCATE, input, multiple);
    }
    public static DensityFunction truncate(DensityFunction input) {
        return round(RoundFunction.Type.TRUNCATE, input);
    }
    public static DensityFunction findTopSurface(DensityFunction density, DensityFunction upperBound, int lowerBound, int stepSize) {
        return JsonDensityFunction.raw("minecraft:find_top_surface", null, "\"density\":" + JsonDensityFunction.of(density) + ",\"upper_bound\":" + JsonDensityFunction.of(upperBound) + ",\"lower_bound\":" + lowerBound + ",\"cell_height\":" + stepSize);
    }
    public static DensityFunction remap(DensityFunction input, float fromMin, float fromMax, float toMin, float toMax) {
        return add(constant(toMin), mul(sub(input, constant(fromMin)), constant((toMax - toMin) / (fromMax - fromMin))));
    }
    public static DensityFunction clampedMap(DensityFunction input, float fromMin, float fromMax, float toMin, float toMax) {
        return clamp(remap(input, fromMin, fromMax, toMin, toMax), Math.min(toMin, toMax), Math.max(toMin, toMax));
    }
    public static record HolderHolder(Holder<DensityFunction> function) implements DensityFunction {
        public DensitySampler compileSampler(DensityFunction.CompileContext context) {
            return function.value().compileSampler(context);
        }
        public DensityFunction rewriteChildren(DfRewriteRule rule) {
            return rule.rewrite(function.value());
        }
        public Interval range() {
            return function.value().range();
        }
        public int domainAxes() {
            return function.value().domainAxes();
        }
        public MapCodec<HolderHolder> codec() {
            return MapCodec.unit(this);
        }
    }
}
