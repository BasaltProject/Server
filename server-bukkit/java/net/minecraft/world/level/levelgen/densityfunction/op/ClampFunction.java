// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.op;
import net.minecraft.world.level.levelgen.densityfunction.SamplerKit;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.world.level.levelgen.densityfunction.JsonDensityFunction;
import net.minecraft.util.Mth;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
public record ClampFunction(DensityFunction input, float min, float max) implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:clamp\",\"input\":" + JsonDensityFunction.of(input) + ",\"min\":" + SamplerKit.number(min) + ",\"max\":" + SamplerKit.number(max) + "}";
    }
    public static final MapCodec<ClampFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.op.ClampFunction.class, "CODEC");
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        return new Sampler(input.compileSampler(context), min, max);
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        DensityFunction rewritten = rule.rewrite(input);
        return rewritten == input ? this : new ClampFunction(rewritten, min, max);
    }
    public MapCodec<ClampFunction> codec() {
        return CODEC;
    }
    public Interval range() {
        return Interval.clamp(input.range(), min, max);
    }
    public int domainAxes() {
        return input.domainAxes();
    }
    public static record Sampler(DensitySampler input, float min, float max) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, input, outputBuffer, volume, v -> Mth.clamp(v, min, max));
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return Mth.clamp(input.sampleValue(context, blockX, blockY, blockZ), min, max);
        }
    }
}
