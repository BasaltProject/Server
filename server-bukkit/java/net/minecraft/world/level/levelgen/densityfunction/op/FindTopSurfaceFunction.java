// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.op;
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
public record FindTopSurfaceFunction(DensityFunction density, DensityFunction upperBound, int lowerBound, int cellHeight) implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:find_top_surface\",\"density\":" + JsonDensityFunction.of(density) + ",\"upper_bound\":" + JsonDensityFunction.of(upperBound) + ",\"lower_bound\":" + lowerBound + ",\"cell_height\":" + cellHeight + "}";
    }
    public static final MapCodec<FindTopSurfaceFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.op.FindTopSurfaceFunction.class, "CODEC");
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        return new SliceFunction.YSampler(new Sampler(density.compileSampler(context), upperBound.compileSampler(context), lowerBound, cellHeight), 0);
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        DensityFunction d = rule.rewrite(density);
        DensityFunction u = rule.rewrite(upperBound);
        return d == density && u == upperBound ? this : new FindTopSurfaceFunction(d, u, lowerBound, cellHeight);
    }
    public Interval range() {
        return Interval.of(lowerBound, Math.max((float) lowerBound, upperBound.range().max()));
    }
    public int domainAxes() {
        return (density.domainAxes() | upperBound.domainAxes()) & ~DensityFunction.AXIS_Y;
    }
    public MapCodec<FindTopSurfaceFunction> codec() {
        return CODEC;
    }
    static record Sampler(DensitySampler density, DensitySampler upperBound, int lowerBound, int cellHeight) implements DensitySampler {
        private float search(SamplerContext context, int x, int z, float top) {
            int probe = Mth.floor(top / (float) cellHeight) * cellHeight;
            for (; probe > lowerBound; probe -= cellHeight) {
                if (density.sampleValue(context, x, probe, z) > 0.0F) {
                    return probe;
                }
            }
            return lowerBound;
        }

        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        if (volume.sizeY() != 1) {
            throw new IllegalArgumentException("A top surface search needs a volume one block high, not " + volume.sizeY());
        }
        upperBound.sampleVolume(context, outputBuffer, volume);
        int index = 0;
        for (int z = 0; z < volume.sizeZ(); z++) {
            for (int x = 0; x < volume.sizeX(); x++, index++) {
                outputBuffer.set(index, search(context, volume.blockX(x), volume.blockZ(z), outputBuffer.get(index)));
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return search(context, blockX, blockZ, upperBound.sampleValue(context, blockX, blockY, blockZ));
        }
    }
}
