// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.op;
import net.minecraft.world.level.levelgen.densityfunction.SamplerKit;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.world.level.levelgen.densityfunction.JsonDensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.generator.ConstantFunction;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
public record PowFunction(DensityFunction base, DensityFunction exponent) implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:pow\",\"base\":" + JsonDensityFunction.of(base) + ",\"exponent\":" + JsonDensityFunction.of(exponent) + "}";
    }
    private static DensitySampler fixedExponent(DensitySampler in, float power) {
        float size = Math.abs(power);
        DensitySampler direct;
        if (size == 0.5F) {
            direct = new UnaryFunction.SqrtSampler(in);
        } else if (size == 1.0F) {
            direct = in;
        } else if (size == 2.0F) {
            direct = new UnaryFunction.SquareSampler(in);
        } else if (size == 3.0F) {
            direct = new UnaryFunction.CubeSampler(in);
        } else {
            return new ConstExponentSampler(in, power);
        }
        return power >= 0.0F ? direct : new UnaryFunction.ReciprocalSampler(direct);
    }
    public static final MapCodec<PowFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.op.PowFunction.class, "CODEC");
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        DensitySampler b = base.compileSampler(context);
        DensitySampler e = exponent.compileSampler(context);
        Float fixedBase = ConstantFunction.valueOf(base);
        if (fixedBase != null) {
            return new ConstBaseSampler(fixedBase, e);
        }
        Float fixedPower = ConstantFunction.valueOf(exponent);
        return fixedPower != null ? fixedExponent(b, fixedPower) : new Sampler(b, e);
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        DensityFunction b = rule.rewrite(base);
        DensityFunction e = rule.rewrite(exponent);
        return b == base && e == exponent ? this : new PowFunction(b, e);
    }
    public Interval range() {
        return Interval.pow(base.range(), exponent.range());
    }
    public int domainAxes() {
        return base.domainAxes() | exponent.domainAxes();
    }
    public MapCodec<PowFunction> codec() {
        return CODEC;
    }
    static record Sampler(DensitySampler base, DensitySampler exponent) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.zip(context, base, exponent, outputBuffer, volume, (b, e) -> (float) Math.pow(b, e));
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        float b = base.sampleValue(context, blockX, blockY, blockZ);
        return (float) Math.pow(b, exponent.sampleValue(context, blockX, blockY, blockZ));
        }
    }
    static record ConstBaseSampler(double base, DensitySampler exponent) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, exponent, outputBuffer, volume, e -> (float) Math.pow(base, e));
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return (float) Math.pow(base, exponent.sampleValue(context, blockX, blockY, blockZ));
        }
    }
    static record ConstExponentSampler(DensitySampler base, double exponent) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, base, outputBuffer, volume, b -> (float) Math.pow(b, exponent));
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return (float) Math.pow(base.sampleValue(context, blockX, blockY, blockZ), exponent);
        }
    }
}
