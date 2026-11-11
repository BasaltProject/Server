// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.op;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.world.level.levelgen.densityfunction.JsonDensityFunction;
import net.minecraft.world.level.levelgen.blending.Blender;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
public record BlendDensityFunction(DensityFunction input) implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:blend_density\",\"input\":" + JsonDensityFunction.of(input) + "}";
    }
    public static final MapCodec<BlendDensityFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.op.BlendDensityFunction.class, "CODEC");
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        return new Sampler(input.compileSampler(context));
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        DensityFunction rewritten = rule.rewrite(input);
        return rewritten == input ? this : new BlendDensityFunction(rewritten);
    }
    public Interval range() {
        return input.range();
    }
    public int domainAxes() {
        return input.domainAxes();
    }
    public MapCodec<BlendDensityFunction> codec() {
        return CODEC;
    }
    static record Sampler(DensitySampler input) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        input.sampleVolume(context, outputBuffer, volume);
        Blender blender = context.getField(Blender.CONTEXT_KEY);
        if (blender == null || blender.isEmpty()) {
            return;
        }
        int index = 0;
        for (int z = 0; z < volume.sizeZ(); z++) {
            for (int x = 0; x < volume.sizeX(); x++) {
                for (int y = 0; y < volume.sizeY(); y++, index++) {
                    outputBuffer.set(index, blender.blendDensity(volume.blockX(x), volume.blockY(y), volume.blockZ(z), outputBuffer.get(index)));
                }
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        float value = input.sampleValue(context, blockX, blockY, blockZ);
        Blender blender = context.getField(Blender.CONTEXT_KEY);
        return blender == null || blender.isEmpty() ? value : blender.blendDensity(blockX, blockY, blockZ, value);
        }
    }
}
