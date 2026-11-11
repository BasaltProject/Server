// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.generator;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Vec3i;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.DistanceMetric;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
public record DistanceToPointFunction(Vec3i point, DistanceMetric metric) implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:distance_to_point\",\"point\":[" + point.getX() + "," + point.getY() + "," + point.getZ() + "],\"metric\":\"" + metric.getSerializedName() + "\"}";
    }
    public static final MapCodec<DistanceToPointFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.generator.DistanceToPointFunction.class, "CODEC");
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        return new Sampler(point.getX(), point.getY(), point.getZ(), metric);
    }
    public Interval range() {
        return Interval.of(0.0F, Float.POSITIVE_INFINITY);
    }
    public int domainAxes() {
        return ALL_AXES;
    }
    public MapCodec<DistanceToPointFunction> codec() {
        return CODEC;
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        return this;
    }
    static record Sampler(int x, int y, int z, DistanceMetric metric) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        DensitySampler.sampleVolumeNaive(context, outputBuffer, volume, this);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return metric.compute((float) (x - blockX), (float) (y - blockY), (float) (z - blockZ));
        }
    }
}
