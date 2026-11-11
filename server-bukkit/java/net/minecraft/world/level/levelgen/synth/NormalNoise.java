// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.synth;

import com.mojang.serialization.Codec;
import it.unimi.dsi.fastutil.doubles.DoubleArrayList;
import it.unimi.dsi.fastutil.doubles.DoubleList;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import net.minecraft.core.Holder;
import net.minecraft.util.Interval;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.PositionalRandomFactory;

public final class NormalNoise {
    private static final double kInputFactor = 1.0181268882175227;
    public static final double TARGET_DEVIATION = 0.3333333333333333;
    public static final Codec<NormalNoise> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(NormalNoise.class, "DIRECT_CODEC");
    public static final Codec<Holder<NormalNoise>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(NormalNoise.class, "CODEC");
    private final Parameters parameters;
    private final List<OctaveInfo> octaves;
    private final double normalizationFactor;
    private final Interval range;

    private NormalNoise(Parameters parameters) {
        this.parameters = parameters;
        this.octaves = buildOctaves(parameters.baseOctave, parameters.baseAmplitude, parameters.octaveCount,
            parameters.normalize != Normalization.DISABLED, parameters.amplitudeModifiers);
        double target = totalAmplitude(octaves);
        double factor = computeNormalizationFactor(target, octaves);
        if (parameters.normalize == Normalization.LEGACY && factor != 0.0) {
            double legacy = computeParityNormalizationFactor(parameters.baseAmplitude, parameters.octaveCount,
                parameters.amplitudeModifiers);
            target *= legacy / factor;
            factor = legacy;
        }
        this.normalizationFactor = factor;
        this.range = Interval.ofSymmetric((float) (target * TARGET_DEVIATION * 6.0));
    }

    private static double totalAmplitude(List<OctaveInfo> octaves) {
        double sum = 0.0;
        for (OctaveInfo octave : octaves) {
            sum += octave.absAmplitude();
        }
        return sum;
    }

    private static List<OctaveInfo> buildOctaves(int baseOctave, double baseAmplitude, int octaveCount,
        boolean normalize, DoubleList modifiers) {
        double frequency = Math.pow(2.0, baseOctave);
        double amplitude = baseAmplitude;
        if (normalize) {
            amplitude = baseAmplitude * (Math.pow(0.5, -(octaveCount - 1)) / (Math.pow(0.5, -octaveCount) - 1.0));
        }
        List<OctaveInfo> out = new ArrayList<>(octaveCount);
        for (int i = 0; i < octaveCount; i++) {
            double modifier = modifierAt(modifiers, i);
            if (modifier != 0.0) {
                out.add(new OctaveInfo(baseOctave + i, frequency, amplitude * modifier));
            }
            frequency *= 2.0;
            amplitude *= 0.5;
        }
        return List.copyOf(out);
    }

    private static double modifierAt(DoubleList modifiers, int index) {
        return modifiers.isEmpty() ? 1.0 : modifiers.getDouble(index);
    }

    private static double computeNormalizationFactor(double targetAmplitude, List<OctaveInfo> octaves) {
        double variance = 0.0;
        for (OctaveInfo octave : octaves) {
            variance += Mth.square(0.2702247831245211 * octave.absAmplitude());
        }
        double deviation = Math.sqrt(variance);
        if (deviation == 0.0) {
            return 0.0;
        }
        return targetAmplitude * TARGET_DEVIATION / (deviation * Math.sqrt(2.0));
    }

    public static Builder builder() {
        return new Builder();
    }

    public static NormalNoise createParity(int firstOctave, DoubleList amplitudes) {
        if (amplitudes.isEmpty()) {
            throw new IllegalArgumentException("Need at least 1 amplitude");
        }
        Builder builder = builder();
        builder.setBaseOctave(firstOctave);
        builder.setOctaveCount(amplitudes.size());
        builder.setBaseAmplitude(computeParityBaseAmplitude(builder.baseOctave, amplitudes));
        for (int i = 0; i < amplitudes.size(); i++) {
            if (amplitudes.getDouble(i) != 1.0) {
                builder.setAmplitudeModifier(i, amplitudes.getDouble(i));
            }
        }
        return builder.build();
    }

    public static NormalNoise createParity(int firstOctave, double... amplitudes) {
        return createParity(firstOctave, DoubleList.of(amplitudes));
    }

    private static double computeParityBaseAmplitude(int baseOctave, DoubleList amplitudes) {
        List<OctaveInfo> built = buildOctaves(baseOctave, 1.0, amplitudes.size(), true, amplitudes);
        double renormalized = computeNormalizationFactor(totalAmplitude(built), built);
        if (renormalized == 0.0) {
            return 1.0;
        }
        return computeParityNormalizationFactor(1.0, amplitudes.size(), amplitudes) / renormalized;
    }

    private static double computeParityNormalizationFactor(double baseAmplitude, int octaveCount, DoubleList modifiers) {
        int lowest = Integer.MAX_VALUE;
        int highest = Integer.MIN_VALUE;
        for (int i = 0; i < octaveCount; i++) {
            if (modifierAt(modifiers, i) != 0.0) {
                lowest = Math.min(lowest, i);
                highest = Math.max(highest, i);
            }
        }
        return baseAmplitude * 0.5 * TARGET_DEVIATION / (0.1 * (1.0 + 1.0 / (highest - lowest + 1)));
    }

    public Noise create(RandomSource random) {
        PositionalRandomFactory first = random.forkPositional();
        PositionalRandomFactory second = random.forkPositional();
        NoiseStack.Builder stack = NoiseStack.builder();
        for (OctaveInfo octave : octaves) {
            float weight = (float) (normalizationFactor * octave.amplitude);
            stack.add(new PerlinNoise(first.fromHashOf(octave.seed())), octave.frequency, weight);
            stack.add(new PerlinNoise(second.fromHashOf(octave.seed())), octave.frequency * kInputFactor, weight);
        }
        return stack.build();
    }

    @Deprecated
    public Noise createForLegacyNetherBiome(RandomSource random) {
        DoubleList amplitudes = parameters.amplitudeModifiers();
        if (amplitudes.isEmpty()) {
            amplitudes = new DoubleArrayList(Collections.nCopies(parameters.octaveCount(), 1.0));
        }
        NoiseStack first = LegacyFbmInitializer.createForLegacyNetherBiome(random, parameters.baseOctave(), amplitudes);
        NoiseStack second = LegacyFbmInitializer.createForLegacyNetherBiome(random, parameters.baseOctave(), amplitudes);
        float weight = (float) (normalizationFactor * parameters.baseAmplitude());
        return NoiseStack.builder().addStack(first, 1.0, weight).addStack(second, kInputFactor, weight).build();
    }

    public Interval range() {
        return range;
    }

    public boolean equals(Object obj) {
        return obj == this || obj instanceof NormalNoise other && parameters.equals(other.parameters);
    }

    public int hashCode() {
        return parameters.hashCode();
    }

    public String toString() {
        return "NormalNoise[parameters=" + parameters + "]";
    }

    public void parityConfigString(StringBuilder sb, RandomSource random) {
        sb.append("NormalNoise {first: ");
        perlinParityConfigString(sb, random);
        sb.append(", second: ");
        perlinParityConfigString(sb, random);
        sb.append("}");
    }

    private void perlinParityConfigString(StringBuilder sb, RandomSource random) {
        List<String> shown = new ArrayList<>();
        for (int i = 0; i < parameters.octaveCount; i++) {
            shown.add(String.format(Locale.ROOT, "%.2f", modifierAt(parameters.amplitudeModifiers, i)));
        }
        sb.append("PerlinNoise{first octave: ").append(parameters.baseOctave).append(", amplitudes: ").append(shown)
            .append(", noise levels: [");
        PositionalRandomFactory positional = random.forkPositional();
        for (int i = 0; i < parameters.octaveCount; i++) {
            sb.append(i).append(": ");
            int wanted = parameters.baseOctave + i;
            OctaveInfo match = null;
            for (OctaveInfo octave : octaves) {
                if (octave.octaveIndex == wanted) {
                    match = octave;
                    break;
                }
            }
            if (match != null) {
                new PerlinNoise(positional.fromHashOf(match.seed())).parityConfigString(sb);
            } else {
                sb.append("null");
            }
            sb.append(", ");
        }
        sb.append("]}");
    }

    static record OctaveInfo(int octaveIndex, double frequency, double amplitude) {
        public String seed() {
            return "octave_" + octaveIndex;
        }

        public double absAmplitude() {
            return Math.abs(amplitude);
        }
    }

    public static class Builder {
        private double baseAmplitude = 1.0;
        private int baseOctave;
        private int octaveCount = 1;
        private Normalization normalize = Normalization.ENABLED;
        private DoubleList amplitudeModifiers = DoubleList.of();

        private Builder() {
        }

        public Builder setBaseAmplitude(double baseAmplitude) {
            this.baseAmplitude = baseAmplitude;
            return this;
        }

        public Builder setBaseOctave(int baseOctave) {
            this.baseOctave = baseOctave;
            return this;
        }

        public Builder setOctaveCount(int octaveCount) {
            if (!amplitudeModifiers.isEmpty()) {
                throw new IllegalArgumentException("Cannot set octave count after setting amplitude modifier");
            }
            this.octaveCount = octaveCount;
            return this;
        }

        public Builder setNormalize(boolean normalize) {
            this.normalize = normalize ? Normalization.ENABLED : Normalization.DISABLED;
            return this;
        }

        @Deprecated
        public Builder setLegacyNormalization() {
            this.normalize = Normalization.LEGACY;
            return this;
        }

        public Builder setAmplitudeModifier(int index, double value) {
            if (index < 0 || index >= octaveCount) {
                throw new IllegalArgumentException(index + " outside of octave range [0; " + octaveCount + ")");
            }
            if (amplitudeModifiers.isEmpty()) {
                amplitudeModifiers = new DoubleArrayList(Collections.nCopies(octaveCount, 1.0));
            }
            amplitudeModifiers.set(index, value);
            return this;
        }

        public NormalNoise build() {
            return new NormalNoise(new Parameters(baseAmplitude, baseOctave, octaveCount, normalize, amplitudeModifiers));
        }
    }

    static record Parameters(double baseAmplitude, int baseOctave, int octaveCount, Normalization normalize,
        DoubleList amplitudeModifiers) {
        public static final Codec<Parameters> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(Parameters.class, "CODEC");
    }

    public static enum Normalization {
        DISABLED, ENABLED, LEGACY;

        public static final Codec<Normalization> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(Normalization.class, "CODEC");
    }
}
