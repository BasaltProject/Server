// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.generator;
import net.minecraft.util.Mth;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
public record ConstantFunction(float value) implements DensityFunction {
    public static Float valueOf(DensityFunction function) {
        return function instanceof ConstantFunction constant ? constant.value() : null;
    }
    public static final MapCodec<ConstantFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.generator.ConstantFunction.class, "CODEC");
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        return new Sampler(value);
    }
    public Interval range() {
        return Interval.ofExact(value);
    }
    public int domainAxes() {
        return NO_AXES;
    }
    public MapCodec<ConstantFunction> codec() {
        return CODEC;
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        return this;
    }
    public static record Sampler(float value) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        outputBuffer.fill(value);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return value;
        }
    }
}
