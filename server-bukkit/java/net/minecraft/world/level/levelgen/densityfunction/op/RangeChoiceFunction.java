// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.op;
import net.minecraft.world.level.levelgen.densityfunction.SamplerKit;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.world.level.levelgen.densityfunction.JsonDensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.ScopedDensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.generator.ConstantFunction;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
public record RangeChoiceFunction(DensityFunction input, float minInclusive, float maxExclusive, DensityFunction whenInRange, DensityFunction whenOutOfRange) implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:range_choice\",\"input\":" + JsonDensityFunction.of(input) + ",\"min_inclusive\":" + SamplerKit.number(minInclusive) + ",\"max_exclusive\":" + SamplerKit.number(maxExclusive) + ",\"when_in_range\":" + JsonDensityFunction.of(whenInRange) + ",\"when_out_of_range\":" + JsonDensityFunction.of(whenOutOfRange) + "}";
    }
    public static final MapCodec<RangeChoiceFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.op.RangeChoiceFunction.class, "CODEC");
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        DensitySampler in = input.compileSampler(context);
        Float inside = ConstantFunction.valueOf(whenInRange);
        Float outside = ConstantFunction.valueOf(whenOutOfRange);
        if (inside != null && outside != null) {
            return new ConstSampler(in, minInclusive, maxExclusive, inside, outside);
        }
        return new Sampler(in, minInclusive, maxExclusive, whenInRange.compileSampler(context), whenOutOfRange.compileSampler(context));
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        DensityFunction a = rule.rewrite(input);
        DensityFunction b = rule.rewrite(whenInRange);
        DensityFunction c = rule.rewrite(whenOutOfRange);
        return a == input && b == whenInRange && c == whenOutOfRange ? this : new RangeChoiceFunction(a, minInclusive, maxExclusive, b, c);
    }
    public Interval range() {
        return Interval.encapsulating(whenInRange.range(), whenOutOfRange.range());
    }
    public int domainAxes() {
        return input.domainAxes() | whenInRange.domainAxes() | whenOutOfRange.domainAxes();
    }
    public MapCodec<RangeChoiceFunction> codec() {
        return CODEC;
    }
    static record Sampler(DensitySampler input, float minInclusive, float maxExclusive, DensitySampler whenInRange, DensitySampler whenOutOfRange) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        whenInRange.sampleVolume(context, outputBuffer, volume);
        try (ScopedDensityBuffer probe = context.acquireBuffer(volume); ScopedDensityBuffer other = context.acquireBuffer(volume)) {
            input.sampleVolume(context, probe, volume);
            whenOutOfRange.sampleVolume(context, other, volume);
            for (int i = 0; i < outputBuffer.size(); i++) {
                float v = probe.get(i);
                if (!(v >= minInclusive && v < maxExclusive)) {
                    outputBuffer.set(i, other.get(i));
                }
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        float v = input.sampleValue(context, blockX, blockY, blockZ);
        return v >= minInclusive && v < maxExclusive
            ? whenInRange.sampleValue(context, blockX, blockY, blockZ)
            : whenOutOfRange.sampleValue(context, blockX, blockY, blockZ);
        }
    }
    static record ConstSampler(DensitySampler input, float minInclusive, float maxExclusive, float whenInRange, float whenOutOfRange) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, input, outputBuffer, volume, v -> v >= minInclusive && v < maxExclusive ? whenInRange : whenOutOfRange);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        float v = input.sampleValue(context, blockX, blockY, blockZ);
        return v >= minInclusive && v < maxExclusive ? whenInRange : whenOutOfRange;

        }
    }
}
