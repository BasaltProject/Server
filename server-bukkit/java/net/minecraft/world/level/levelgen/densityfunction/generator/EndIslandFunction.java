// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.generator;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
import net.minecraft.world.level.levelgen.synth.SimplexNoise;
public record EndIslandFunction() implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:end_outer_islands\"}";
    }
    private static float heightAt(SimplexNoise noise, int sectionX, int sectionZ) {
        int chunkX = sectionX / 2;
        int chunkZ = sectionZ / 2;
        int subX = sectionX % 2;
        int subZ = sectionZ % 2;
        float best = -100.0F;
        for (int dx = -12; dx <= 12; dx++) {
            for (int dz = -12; dz <= 12; dz++) {
                long cx = chunkX + dx;
                long cz = chunkZ + dz;
                if (cx * cx + cz * cz > 4096L && noise.get((double) cx, (double) cz) < -0.9F) {
                    float size = (Mth.abs((float) cx) * 3439.0F + Mth.abs((float) cz) * 147.0F) % 13.0F + 9.0F;
                    float ox = (float) (subX - dx * 2);
                    float oz = (float) (subZ - dz * 2);
                    float reach = Mth.clamp(100.0F - Mth.sqrt(ox * ox + oz * oz) * size, -100.0F, 80.0F);
                    best = Math.max(best, reach);
                }
            }
        }
        return best;
    }
    public static final MapCodec<EndIslandFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.generator.EndIslandFunction.class, "CODEC");
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        RandomSource random = context.createEndIslandRandom();
        random.consumeCount(17292);
        return new Sampler(new SimplexNoise(random, true));
    }
    public Interval range() {
        return Interval.of(-0.84375F, 0.5625F);
    }
    public int domainAxes() {
        return AXIS_X | AXIS_Z;
    }
    public MapCodec<EndIslandFunction> codec() {
        return CODEC;
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        return this;
    }
    static record Sampler(SimplexNoise islandNoise) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        for (int z = 0; z < volume.sizeZ(); z++) {
            for (int x = 0; x < volume.sizeX(); x++) {
                outputBuffer.setRange(volume.indexUnchecked(x, 0, z), volume.sizeY(), sampleValue(context, volume.blockX(x), 0, volume.blockZ(z)));
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return (heightAt(islandNoise, blockX / 8, blockZ / 8) - 8.0F) / 128.0F;

        }
    }
}
