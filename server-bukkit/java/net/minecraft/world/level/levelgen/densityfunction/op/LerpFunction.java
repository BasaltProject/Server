// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.op;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.world.level.levelgen.densityfunction.JsonDensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.ScopedDensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.generator.ConstantFunction;
import net.minecraft.util.Mth;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
public record LerpFunction(DensityFunction alpha, DensityFunction first, DensityFunction second) implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:lerp\",\"alpha\":" + JsonDensityFunction.of(alpha) + ",\"first\":" + JsonDensityFunction.of(first) + ",\"second\":" + JsonDensityFunction.of(second) + "}";
    }
    private static float blend(float a, float x, float y) {
        return a == 0.0F ? x : a == 1.0F ? y : Mth.lerp(a, x, y);
    }
    public static final MapCodec<LerpFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.op.LerpFunction.class, "CODEC");
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        DensitySampler a = alpha.compileSampler(context);
        DensitySampler f = first.compileSampler(context);
        DensitySampler s = second.compileSampler(context);
        Float fc = ConstantFunction.valueOf(first);
        Float sc = ConstantFunction.valueOf(second);
        return fc != null ? new ConstFirstSampler(a, fc, s) : sc != null ? new ConstSecondSampler(a, f, sc) : new Sampler(a, f, s);
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        DensityFunction a = rule.rewrite(alpha);
        DensityFunction f = rule.rewrite(first);
        DensityFunction s = rule.rewrite(second);
        return a == alpha && f == first && s == second ? this : new LerpFunction(a, f, s);
    }
    public Interval range() {
        return Interval.lerp(alpha.range(), first.range(), second.range());
    }
    public int domainAxes() {
        return alpha.domainAxes() | first.domainAxes() | second.domainAxes();
    }
    public MapCodec<LerpFunction> codec() {
        return CODEC;
    }
    public static record Sampler(DensitySampler alpha, DensitySampler first, DensitySampler second) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        alpha.sampleVolume(context, outputBuffer, volume);
        try (ScopedDensityBuffer low = context.acquireBuffer(volume); ScopedDensityBuffer high = context.acquireBuffer(volume)) {
            first.sampleVolume(context, low, volume);
            second.sampleVolume(context, high, volume);
            for (int i = 0; i < outputBuffer.size(); i++) {
                outputBuffer.set(i, blend(outputBuffer.get(i), low.get(i), high.get(i)));
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        float a = alpha.sampleValue(context, blockX, blockY, blockZ);
        if (a == 0.0F) {
            return first.sampleValue(context, blockX, blockY, blockZ);
        }
        if (a == 1.0F) {
            return second.sampleValue(context, blockX, blockY, blockZ);
        }
        float low = first.sampleValue(context, blockX, blockY, blockZ);
        return Mth.lerp(a, low, second.sampleValue(context, blockX, blockY, blockZ));
        }
    }
    public static record ConstFirstSampler(DensitySampler alpha, float first, DensitySampler second) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        alpha.sampleVolume(context, outputBuffer, volume);
        try (ScopedDensityBuffer high = context.acquireBuffer(volume)) {
            second.sampleVolume(context, high, volume);
            for (int i = 0; i < outputBuffer.size(); i++) {
                outputBuffer.set(i, blend(outputBuffer.get(i), first, high.get(i)));
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        float a = alpha.sampleValue(context, blockX, blockY, blockZ);
        return a == 0.0F ? first : blend(a, first, second.sampleValue(context, blockX, blockY, blockZ));
        }
    }
    public static record ConstSecondSampler(DensitySampler alpha, DensitySampler first, float second) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        alpha.sampleVolume(context, outputBuffer, volume);
        try (ScopedDensityBuffer low = context.acquireBuffer(volume)) {
            first.sampleVolume(context, low, volume);
            for (int i = 0; i < outputBuffer.size(); i++) {
                outputBuffer.set(i, blend(outputBuffer.get(i), low.get(i), second));
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        float a = alpha.sampleValue(context, blockX, blockY, blockZ);
        if (a == 0.0F) {
            return first.sampleValue(context, blockX, blockY, blockZ);
        }
        return a == 1.0F ? second : Mth.lerp(a, first.sampleValue(context, blockX, blockY, blockZ), second);
        }
    }
}
