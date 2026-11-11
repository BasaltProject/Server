// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.generator;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.util.Mth;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Direction;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
import net.minecraft.world.level.levelgen.densityfunction.TilingMode;
public record GradientFunction(Direction.Axis axis, TilingMode tiling, int fromCoordinate, int toCoordinate, float fromValue, float toValue) implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:gradient\",\"axis\":\"" + axis.getSerializedName() + "\",\"tiling\":\"" + tiling.getSerializedName() + "\",\"from_coordinate\":" + fromCoordinate + ",\"to_coordinate\":" + toCoordinate + ",\"from_value\":" + net.minecraft.world.level.levelgen.densityfunction.SamplerKit.number(fromValue) + ",\"to_value\":" + net.minecraft.world.level.levelgen.densityfunction.SamplerKit.number(toValue) + "}";
    }
    public static final MapCodec<GradientFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.generator.GradientFunction.class, "CODEC");
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        int span = toCoordinate - fromCoordinate;
        float slope = (toValue - fromValue) / (float) span;
        return switch (tiling) {
            case CLAMP_TO_EDGE -> new ClampedSampler(axis, fromCoordinate, Math.min(fromCoordinate, toCoordinate), Math.max(fromCoordinate, toCoordinate), fromValue, slope);
            case REPEAT -> new RepeatSampler(axis, fromCoordinate, span, fromValue, slope);
            case MIRRORED_REPEAT -> new MirroredRepeatSampler(axis, fromCoordinate, span, fromValue, slope);
        };
    }
    public Interval range() {
        return Interval.encapsulating(fromValue, toValue);
    }
    public int domainAxes() {
        return DensityFunction.axesFrom(axis);
    }
    public MapCodec<GradientFunction> codec() {
        return CODEC;
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        return this;
    }
    interface GradientSampler extends DensitySampler {
        static void sampleVolume(GradientSampler sampler, DensityBuffer output, DensityVolume volume) {
        switch (sampler.axis()) {
            case X:
                for (int x = 0; x < volume.sizeX(); x++) {
                    float value = sampler.compute(volume.blockX(x));
                    for (int z = 0; z < volume.sizeZ(); z++) {
                        output.setRange(volume.indexUnchecked(x, 0, z), volume.sizeY(), value);
                    }
                }
                break;
            case Y:
                for (int y = 0; y < volume.sizeY(); y++) {
                    float value = sampler.compute(volume.blockY(y));
                    for (int z = 0; z < volume.sizeZ(); z++) {
                        for (int x = 0; x < volume.sizeX(); x++) {
                            output.set(volume.indexUnchecked(x, y, z), value);
                        }
                    }
                }
                break;
            default:
                for (int z = 0; z < volume.sizeZ(); z++) {
                    output.setRange(volume.indexUnchecked(0, 0, z), volume.sizeX() * volume.sizeY(), sampler.compute(volume.blockZ(z)));
                }
        }
        }
        Direction.Axis axis();
        float compute(int coordinate);
    }
    static record ClampedSampler(Direction.Axis axis, int fromCoordinate, int minCoordinate, int maxCoordinate, float fromValue, float coordinateFactor) implements GradientSampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        GradientSampler.sampleVolume(this, outputBuffer, volume);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return compute(axis.choose(blockX, blockY, blockZ));
        }
        public float compute(int coordinate) {
        return fromValue + (float) (Mth.clamp(coordinate, minCoordinate, maxCoordinate) - fromCoordinate) * coordinateFactor;
        }
    }
    static record RepeatSampler(Direction.Axis axis, int fromCoordinate, int coordinateRange, float fromValue, float coordinateFactor) implements GradientSampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        GradientSampler.sampleVolume(this, outputBuffer, volume);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return compute(axis.choose(blockX, blockY, blockZ));
        }
        public float compute(int coordinate) {
        return fromValue + (float) Math.floorMod(coordinate - fromCoordinate, coordinateRange) * coordinateFactor;
        }
    }
    static record MirroredRepeatSampler(Direction.Axis axis, int fromCoordinate, int coordinateRange, float fromValue, float coordinateFactor) implements GradientSampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        GradientSampler.sampleVolume(this, outputBuffer, volume);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return compute(axis.choose(blockX, blockY, blockZ));
        }
        public float compute(int coordinate) {
        int offset = coordinate - fromCoordinate;
        int tile = Math.floorDiv(offset, coordinateRange);
        int within = offset - tile * coordinateRange;
        int steps = (tile & 1) == 0 ? within : coordinateRange - within;
        return fromValue + (float) steps * coordinateFactor;
        }
    }
}
