// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.synth;

import com.mojang.serialization.MapCodec;
import net.minecraft.resources.Identifier;
import net.minecraft.util.Interval;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.world.level.levelgen.densityfunction.generator.NoiseFunction;
import net.minecraft.world.level.levelgen.densityfunction.op.BinaryFunction;
import net.minecraft.world.level.levelgen.densityfunction.op.ClampFunction;
import net.minecraft.world.level.levelgen.densityfunction.op.LerpFunction;

public record BlendedNoise(double xzScale, double yScale, double xzFactor, double yFactor, double smearScaleMultiplier)
    implements DensityFunction, TreeSource {
    public static final MapCodec<BlendedNoise> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(BlendedNoise.class, "CODEC");
    private static final double kBaseScale = 684.412;
    private static final double kLimitFactor = 0.99998474F;
    private static final double kMainFactor = 12.75F;
    private static final int kLimitFirstOctave = -15;
    private static final int kMainFirstOctave = -7;
    public static final Identifier NOISE_SEED = Identifier.withDefaultNamespace("terrain");

    public String treeJson() {
        return "{\"type\":\"minecraft:old_blended_noise\",\"xz_scale\":" + xzScale + ",\"y_scale\":" + yScale
            + ",\"xz_factor\":" + xzFactor + ",\"y_factor\":" + yFactor + ",\"smear_scale_multiplier\":"
            + smearScaleMultiplier + "}";
    }

    private double xzMultiplier() {
        return kBaseScale * xzScale;
    }

    private double yMultiplier() {
        return kBaseScale * yScale;
    }

    public FbmSet createFbmSet(RandomSource random) {
        double limitSmear = yMultiplier() * smearScaleMultiplier;
        double mainSmear = limitSmear / yFactor;
        NoiseStack low = createFbm(random, kLimitFirstOctave, limitSmear, kLimitFactor);
        NoiseStack high = createFbm(random, kLimitFirstOctave, limitSmear, kLimitFactor);
        return new FbmSet(low, high, createFbm(random, kMainFirstOctave, mainSmear, kMainFactor));
    }

    public static NoiseStack createFbm(RandomSource random, int firstOctave, double smearScaleY, double valueFactor) {
        if (firstOctave > 0) {
            throw new IllegalArgumentException("firstOctave>0");
        }
        int count = -firstOctave + 1;
        double factor = 1.0;
        double weight = valueFactor / (Math.pow(2.0, count) - 1.0);
        NoiseStack.Builder stack = NoiseStack.builder();
        for (int i = count - 1; i >= 0; i--) {
            stack.add(new SmearedPerlinNoise(random, smearScaleY * factor), factor, (float) weight);
            factor /= 2.0;
            weight *= 2.0;
        }
        return stack.build();
    }

    private static Interval computeFbmRange(int firstOctave, double smearScaleY, double valueFactor) {
        int count = -firstOctave + 1;
        double factor = 1.0;
        double weight = valueFactor / (Math.pow(2.0, count) - 1.0);
        Interval sum = Interval.ofExact(0.0F);
        for (int i = count - 1; i >= 0; i--) {
            sum = Interval.add(sum, Interval.mul(SmearedPerlinNoise.range(smearScaleY * factor),
                Interval.ofExact((float) weight)));
            factor /= 2.0;
            weight *= 2.0;
        }
        return sum;
    }

    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        return compileSampler(context.createRandom(NOISE_SEED));
    }

    public DensitySampler compileSampler(RandomSource random) {
        FbmSet sets = createFbmSet(random);
        double xz = xzMultiplier();
        double y = yMultiplier();
        DensitySampler low = new NoiseFunction.Sampler(sets.minLimitNoise(), xz, y);
        DensitySampler high = new NoiseFunction.Sampler(sets.maxLimitNoise(), xz, y);
        DensitySampler main = new NoiseFunction.Sampler(sets.mainNoise(), xz / xzFactor, y / yFactor);
        DensitySampler choice = new ClampFunction.Sampler(new BinaryFunction.ConstAddSampler(main, 0.5F), 0.0F, 1.0F);
        return new LerpFunction.Sampler(choice, low, high);
    }

    public Interval range() {
        return computeFbmRange(kLimitFirstOctave, yMultiplier() * smearScaleMultiplier, kLimitFactor);
    }

    public int domainAxes() {
        return 7;
    }

    public MapCodec<BlendedNoise> codec() {
        return CODEC;
    }

    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        return this;
    }

    public static record FbmSet(NoiseStack minLimitNoise, NoiseStack maxLimitNoise, NoiseStack mainNoise) {
        public void parityConfigString(StringBuilder sb) {
            sb.append("BlendedNoise{minLimitNoise=");
            describe(sb, minLimitNoise);
            sb.append(", maxLimitNoise=");
            describe(sb, maxLimitNoise);
            sb.append(", mainNoise=");
            describe(sb, mainNoise);
            sb.append(String.format(java.util.Locale.ROOT, ", xzScale=%.3f, yScale=%.3f, xzMainScale=%.3f, "
                + "yMainScale=%.3f, cellWidth=4, cellHeight=8", 684.412, 684.412, 8.555150000000001, 4.277575000000001))
                .append('}');
        }

        private static void describe(StringBuilder out, NoiseStack noise) {
            int count = noise.layers.length;
            out.append("PerlinNoise{first octave: ").append(-count + 1).append(", amplitudes: [");
            for (int i = 0; i < count; i++) {
                out.append(i == 0 ? "" : ", ").append("1.00");
            }
            out.append("], noise levels: [");
            for (int i = 0; i < count; i++) {
                out.append(i).append(": ");
                ((SmearedPerlinNoise) noise.getLayer(count - 1 - i)).parityConfigString(out);
                out.append(", ");
            }
            out.append("]}");
        }
    }
}
