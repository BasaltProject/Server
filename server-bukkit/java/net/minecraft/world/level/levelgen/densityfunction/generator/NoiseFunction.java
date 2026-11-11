// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.generator;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.world.level.levelgen.densityfunction.JsonDensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunctions;
import net.minecraft.world.level.levelgen.densityfunction.ScopedDensityBuffer;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
import net.minecraft.world.level.levelgen.synth.Noise;
import net.minecraft.world.level.levelgen.synth.NormalNoise;
public record NoiseFunction(Holder<NormalNoise> noise, double xzScale, double yScale, DensityFunction shiftX, DensityFunction shiftY, DensityFunction shiftZ) implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:noise\",\"noise\":" + JsonDensityFunction.key(noise) + ",\"xz_scale\":" + xzScale + ",\"y_scale\":" + yScale
            + ",\"shift_x\":" + JsonDensityFunction.of(shiftX) + ",\"shift_y\":" + JsonDensityFunction.of(shiftY) + ",\"shift_z\":" + JsonDensityFunction.of(shiftZ) + "}";
    }
    private static boolean isZero(DensityFunction function) {
        Float value = ConstantFunction.valueOf(function);
        return value != null && value == 0.0F;
    }
    public static final MapCodec<NoiseFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.generator.NoiseFunction.class, "CODEC");
    public NoiseFunction(Holder<NormalNoise> noise, double xzScale, double yScale, DensityFunction shiftX, DensityFunction shiftY, DensityFunction shiftZ) {
        this.noise = noise;
        this.xzScale = xzScale;
        this.yScale = yScale;
        this.shiftX = shiftX;
        this.shiftY = shiftY;
        this.shiftZ = shiftZ;
    }
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        Noise sampled = context.createNoiseSampler(noise);
        if (isZero(shiftX) && isZero(shiftY) && isZero(shiftZ)) {
            return new Sampler(sampled, xzScale, yScale);
        }
        DensitySampler sx = shiftX.compileSampler(context);
        DensitySampler sz = shiftZ.compileSampler(context);
        if (isZero(shiftY)) {
            return new ShiftedXzSampler(sx, sz, sampled, xzScale, yScale);
        }
        return new ShiftedXyzSampler(sx, shiftY.compileSampler(context), sz, sampled, xzScale, yScale);
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        DensityFunction x = rule.rewrite(shiftX);
        DensityFunction y = rule.rewrite(shiftY);
        DensityFunction z = rule.rewrite(shiftZ);
        return x == shiftX && y == shiftY && z == shiftZ ? this : new NoiseFunction(noise, xzScale, yScale, x, y, z);
    }
    public Interval range() {
        return noise.value().range();
    }
    public int domainAxes() {
        int axes = ALL_AXES;
        if (yScale == 0.0) {
            axes &= ~AXIS_Y;
        }
        if (xzScale == 0.0) {
            axes &= ~(AXIS_X | AXIS_Z);
        }
        return axes | shiftX.domainAxes() | shiftY.domainAxes() | shiftZ.domainAxes();
    }
    public MapCodec<NoiseFunction> codec() {
        return CODEC;
    }
    public double xzScale() {
        return xzScale;
    }
    public static record Sampler(Noise noise, double xzScale, double yScale) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        outputBuffer.fill(0.0F);
        noise.addToVolume(outputBuffer, volume, xzScale, yScale, 1.0F);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return noise.get((double) blockX * xzScale, (double) blockY * yScale, (double) blockZ * xzScale);
        }
    }
    public static record ShiftedXzSampler(DensitySampler shiftX, DensitySampler shiftZ, Noise noise, double xzScale, double yScale) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        shiftX.sampleVolume(context, outputBuffer, volume);
        try (ScopedDensityBuffer offsetZ = context.acquireBuffer(volume)) {
            shiftZ.sampleVolume(context, offsetZ, volume);
            int index = 0;
            for (int z = 0; z < volume.sizeZ(); z++) {
                for (int x = 0; x < volume.sizeX(); x++) {
                    for (int y = 0; y < volume.sizeY(); y++, index++) {
                        outputBuffer.set(index, noise.get((double) volume.blockX(x) * xzScale + (double) outputBuffer.get(index),
                            (double) volume.blockY(y) * yScale, (double) volume.blockZ(z) * xzScale + (double) offsetZ.get(index)));
                    }
                }
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        double nx = (double) blockX * xzScale + (double) shiftX.sampleValue(context, blockX, blockY, blockZ);
        double nz = (double) blockZ * xzScale + (double) shiftZ.sampleValue(context, blockX, blockY, blockZ);
        return noise.get(nx, (double) blockY * yScale, nz);
        }
    }
    public static record ShiftedXyzSampler(DensitySampler shiftX, DensitySampler shiftY, DensitySampler shiftZ, Noise noise, double xzScale, double yScale) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        shiftX.sampleVolume(context, outputBuffer, volume);
        try (ScopedDensityBuffer offsetY = context.acquireBuffer(volume); ScopedDensityBuffer offsetZ = context.acquireBuffer(volume)) {
            shiftY.sampleVolume(context, offsetY, volume);
            shiftZ.sampleVolume(context, offsetZ, volume);
            int index = 0;
            for (int z = 0; z < volume.sizeZ(); z++) {
                for (int x = 0; x < volume.sizeX(); x++) {
                    for (int y = 0; y < volume.sizeY(); y++, index++) {
                        outputBuffer.set(index, noise.get((double) volume.blockX(x) * xzScale + (double) outputBuffer.get(index),
                            (double) volume.blockY(y) * yScale + (double) offsetY.get(index),
                            (double) volume.blockZ(z) * xzScale + (double) offsetZ.get(index)));
                    }
                }
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        double nx = (double) blockX * xzScale + (double) shiftX.sampleValue(context, blockX, blockY, blockZ);
        double ny = (double) blockY * yScale + (double) shiftY.sampleValue(context, blockX, blockY, blockZ);
        double nz = (double) blockZ * xzScale + (double) shiftZ.sampleValue(context, blockX, blockY, blockZ);
        return noise.get(nx, ny, nz);
        }
    }
}
