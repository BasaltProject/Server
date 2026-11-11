// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.op;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.world.level.levelgen.densityfunction.JsonDensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.SamplerKit;
import net.minecraft.world.level.levelgen.densityfunction.ScopedDensityBuffer;
import java.util.HashMap;
import java.util.Map;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.BoundedFloatFunction;
import net.minecraft.util.CubicSpline;
import net.minecraft.util.Interval;
import net.minecraft.util.Unit;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
public record SplineFunction(CubicSpline<Coordinate> spline) implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:spline\",\"spline\":" + splineJson(spline) + "}";
    }
    private static String splineJson(CubicSpline<Coordinate> node) {
        if (node instanceof CubicSpline.Constant<Coordinate> fixed) {
            return SamplerKit.number(fixed.value());
        }
        CubicSpline.Multipoint<Coordinate> multi = (CubicSpline.Multipoint<Coordinate>) node;
        StringBuilder out = new StringBuilder("{\"coordinate\":").append(JsonDensityFunction.of(multi.coordinate().function())).append(",\"points\":[");
        for (int i = 0; i < multi.locations().length; i++) {
            out.append(i == 0 ? "" : ",").append("{\"location\":").append(SamplerKit.number(multi.locations()[i]))
                .append(",\"value\":").append(splineJson(multi.values().get(i)))
                .append(",\"derivative\":").append(SamplerKit.number(multi.derivatives()[i])).append('}');
        }
        return out.append("]}").toString();
    }
    public static final MapCodec<SplineFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.op.SplineFunction.class, "CODEC");
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        return new Sampler(context, spline);
    }
    public Interval range() {
        return spline.range();
    }
    public int domainAxes() {
        int[] axes = {0};
        spline.forEachCoordinate(coordinate -> axes[0] |= coordinate.function().domainAxes());
        return axes[0];
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        boolean[] changed = {false};
        CubicSpline<Coordinate> mapped = spline.mapCoordinates(coordinate -> {
            DensityFunction rewritten = rule.rewrite(coordinate.function());
            if (rewritten == coordinate.function()) {
                return coordinate;
            }
            changed[0] = true;
            return new Coordinate(rewritten);
        });
        return changed[0] ? new SplineFunction(mapped) : this;
    }
    public MapCodec<SplineFunction> codec() {
        return CODEC;
    }
    public static record Coordinate(DensityFunction function) implements BoundedFloatFunction<Unit> {
        public static final Codec<Coordinate> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.densityfunction.op.SplineFunction.Coordinate.class, "CODEC");
        public float apply(Unit point) {
        return 0.0F;
        }
        public Interval range() {
        return function.range();
        }
    }
    static class Sampler implements DensitySampler {
        private final net.minecraft.util.BoundedFloatFunction<SplineInput> evaluator;
        private final int coordinateCount;
        public Sampler(DensityFunction.CompileContext context, CubicSpline<Coordinate> spline) {
        Map<DensityFunction, SamplerCoordinate> known = new HashMap<>();
        this.evaluator = CubicSpline.asSampler(spline.mapCoordinates(coordinate -> known.computeIfAbsent(coordinate.function(), function ->
            new SamplerCoordinate(function.compileSampler(context), known.size(), function.range()))));
        this.coordinateCount = known.size();
        }
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        try (BufferSplineInput input = new BufferSplineInput(context, volume, coordinateCount)) {
            for (int i = 0; i < outputBuffer.size(); i++) {
                input.slot = i;
                outputBuffer.set(i, evaluator.apply(input));
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return evaluator.apply(new PointSplineInput(context, blockX, blockY, blockZ, coordinateCount));
        }
    }
    static record SamplerCoordinate(DensitySampler sampler, int index, Interval range) implements BoundedFloatFunction<SplineInput> {
        public float apply(SplineInput input) {
        return input.sampleCoordinate(sampler, index);
        }
    }
    static class BufferSplineInput implements SplineInput, AutoCloseable {
        private final SamplerContext context;
        private final DensityVolume volume;
        private final ScopedDensityBuffer[] buffers;
        int slot;
        private BufferSplineInput(SamplerContext context, DensityVolume volume, int coordinateCount) {
        this.context = context;
        this.volume = volume;
        this.buffers = new ScopedDensityBuffer[coordinateCount];
        }
        public float sampleCoordinate(DensitySampler coordinateSampler, int coordinateIndex) {
        ScopedDensityBuffer held = buffers[coordinateIndex];
        if (held == null) {
            held = context.acquireBuffer(volume);
            coordinateSampler.sampleVolume(context, held, volume);
            buffers[coordinateIndex] = held;
        }
        return held.get(slot);
        }
        public void close() {
        for (ScopedDensityBuffer held : buffers) {
            if (held != null) {
                held.close();
            }
        }
        }
    }
    static class PointSplineInput implements SplineInput {
        private final SamplerContext context;
        private final int x;
        private final int y;
        private final int z;
        private final float[] seen;
        private PointSplineInput(SamplerContext context, int blockX, int blockY, int blockZ, int coordinateCount) {
        this.context = context;
        this.x = blockX;
        this.y = blockY;
        this.z = blockZ;
        this.seen = new float[coordinateCount];
        java.util.Arrays.fill(seen, Float.NaN);
        }
        public float sampleCoordinate(DensitySampler coordinateSampler, int coordinateIndex) {
        if (Float.isNaN(seen[coordinateIndex])) {
            seen[coordinateIndex] = coordinateSampler.sampleValue(context, x, y, z);
        }
        return seen[coordinateIndex];

        }
    }
    interface SplineInput {
        float sampleCoordinate(DensitySampler coordinateSampler, int coordinateIndex);
    }
}
