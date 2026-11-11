// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.generator;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.world.level.levelgen.densityfunction.JsonDensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.ScopedDensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.op.BinaryFunction;
import net.minecraft.world.level.levelgen.synth.Noise;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.synth.NormalNoise;
public interface ShiftNoiseFunction extends DensityFunction, TreeSource {
    default String treeJson() {
        String type = this instanceof ShiftA ? "shift_a" : this instanceof ShiftB ? "shift_b" : "shift";
        return "{\"type\":\"minecraft:" + type + "\",\"noise\":" + JsonDensityFunction.key(offsetNoise()) + "}";
    }
    double COORDINATE_FACTOR = 0.25;
    float VALUE_FACTOR = 4.0F;
    Holder<NormalNoise> offsetNoise();
    default Interval range() {
        return Interval.mul(offsetNoise().value().range(), Interval.ofExact(VALUE_FACTOR));
    }
    MapCodec<? extends ShiftNoiseFunction> codec();
    public static record Shift(Holder<NormalNoise> offsetNoise) implements ShiftNoiseFunction {
        public static final MapCodec<Shift> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.generator.ShiftNoiseFunction.Shift.class, "CODEC");
        public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        return new BinaryFunction.ConstMulSampler(new NoiseFunction.Sampler(context.createNoiseSampler(offsetNoise), 0.25, 0.25), 4.0F);
        }
        public DensityFunction rewriteChildren(DfRewriteRule rule) {
        return this;
        }
        public int domainAxes() {
        return ALL_AXES;
        }
        public MapCodec<Shift> codec() {
            return CODEC;
        }
    }
    public static record ShiftA(Holder<NormalNoise> offsetNoise) implements ShiftNoiseFunction {
        public static final MapCodec<ShiftA> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.generator.ShiftNoiseFunction.ShiftA.class, "CODEC");
        public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        return new BinaryFunction.ConstMulSampler(new NoiseFunction.Sampler(context.createNoiseSampler(offsetNoise), 0.25, 0.0), 4.0F);
        }
        public DensityFunction rewriteChildren(DfRewriteRule rule) {
        return this;
        }
        public int domainAxes() {
        return AXIS_X | AXIS_Z;
        }
        public MapCodec<ShiftA> codec() {
            return CODEC;
        }
    }
    public static record ShiftB(Holder<NormalNoise> offsetNoise) implements ShiftNoiseFunction {
        public static final MapCodec<ShiftB> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.generator.ShiftNoiseFunction.ShiftB.class, "CODEC");
        public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        Noise shifting = context.createNoiseSampler(offsetNoise);
        return new DensitySampler() {
            public void sampleVolume(SamplerContext sampler, DensityBuffer outputBuffer, DensityVolume volume) {
                DensityVolume swapped = new DensityVolume(volume.sizeZ(), volume.sizeX(), 1, volume.minBlockZ(), volume.minBlockX(), 0, volume.stepBlockZ(), volume.stepBlockX(), 1);
                try (ScopedDensityBuffer plane = sampler.acquireBuffer(swapped)) {
                    plane.fill(0.0F);
                    shifting.addToVolume(plane, swapped, 0.25, 0.25, 4.0F);
                    for (int z = 0; z < volume.sizeZ(); z++) {
                        for (int x = 0; x < volume.sizeX(); x++) {
                            outputBuffer.setRange(volume.indexUnchecked(x, 0, z), volume.sizeY(), plane.get(swapped.indexUnchecked(z, x, 0)));
                        }
                    }
                }
            }
            public float sampleValue(SamplerContext sampler, int blockX, int blockY, int blockZ) {
                return shifting.get((double) blockZ * 0.25, (double) blockX * 0.25, 0.0) * 4.0F;
            }
        };
        }
        public DensityFunction rewriteChildren(DfRewriteRule rule) {
        return this;
        }
        public int domainAxes() {
        return AXIS_X | AXIS_Z;
        }
        public MapCodec<ShiftB> codec() {
            return CODEC;
        }
    }
}
