// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.op;
import net.minecraft.world.level.levelgen.densityfunction.SamplerKit;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.world.level.levelgen.densityfunction.JsonDensityFunction;
import net.minecraft.util.Mth;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
public record UnaryFunction(Type type, DensityFunction input) implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:" + type.id + "\",\"input\":" + JsonDensityFunction.of(input) + "}";
    }
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        DensitySampler in = input.compileSampler(context);
        return switch (type) {
            case ABS -> new AbsSampler(in);
            case SQUARE -> new SquareSampler(in);
            case CUBE -> new CubeSampler(in);
            case SQRT -> new SqrtSampler(in);
            case HALF_NEGATIVE -> new LeakyReLUSampler(in, 0.5F);
            case QUARTER_NEGATIVE -> new LeakyReLUSampler(in, 0.25F);
            case RECIPROCAL -> new ReciprocalSampler(in);
            case NEGATE -> new NegateSampler(in);
            case SQUEEZE -> new SqueezeSampler(in);
            case LOG -> new LogSampler(in);
            case SIGN -> new SignSampler(in);
        };
    }
    public UnaryFunction rewriteChildren(DfRewriteRule rule) {
        DensityFunction rewritten = rule.rewrite(input);
        return rewritten == input ? this : new UnaryFunction(type, rewritten);
    }
    public MapCodec<UnaryFunction> codec() {
        return type.codec;
    }
    public Interval range() {
        Interval in = input.range();
        return switch (type) {
            case ABS -> Interval.abs(in);
            case SQUARE -> Interval.square(in);
            case CUBE -> Interval.mapMonotonic(in, Mth::cube);
            case SQRT -> Interval.pow(in, Interval.ofExact(0.5F));
            case HALF_NEGATIVE -> Interval.mapMonotonic(in, v -> LeakyReLUSampler.apply(0.5F, v));
            case QUARTER_NEGATIVE -> Interval.mapMonotonic(in, v -> LeakyReLUSampler.apply(0.25F, v));
            case RECIPROCAL -> Interval.reciprocal(in);
            case NEGATE -> Interval.sub(Interval.ofExact(0.0F), in);
            case SQUEEZE -> Interval.mapMonotonic(in, SqueezeSampler::apply);
            case LOG -> Interval.log(in);
            case SIGN -> Interval.sign(in);
        };
    }
    public int domainAxes() {
        return input.domainAxes();
    }
    public static enum Type {
        ABS("abs"), SQUARE("square"), CUBE("cube"), SQRT("sqrt"), HALF_NEGATIVE("half_negative"), QUARTER_NEGATIVE("quarter_negative"), RECIPROCAL("reciprocal"), NEGATE("negate"), SQUEEZE("squeeze"), LOG("log"), SIGN("sign");
        public final String id;
        public final MapCodec<UnaryFunction> codec;
        Type(String id) {
            this.id = id;
            this.codec = RecordCodecBuilder.<UnaryFunction>mapCodec(i -> i.group(
                DensityFunction.CODEC.fieldOf("input").forGetter(UnaryFunction::input)).apply(i, in -> new UnaryFunction(this, in)));
        }

    }
    public static record AbsSampler(DensitySampler input) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, input, outputBuffer, volume, Math::abs);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return Math.abs(input.sampleValue(context, blockX, blockY, blockZ));
        }
    }
    public static record SquareSampler(DensitySampler input) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, input, outputBuffer, volume, v -> v * v);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        float v = input.sampleValue(context, blockX, blockY, blockZ);
        return v * v;
        }
    }
    public static record SqrtSampler(DensitySampler input) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, input, outputBuffer, volume, v -> (float) Math.sqrt(v));
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return (float) Math.sqrt(input.sampleValue(context, blockX, blockY, blockZ));
        }
    }
    public static record ReciprocalSampler(DensitySampler input) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, input, outputBuffer, volume, v -> 1.0F / v);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return 1.0F / input.sampleValue(context, blockX, blockY, blockZ);
        }
    }
    public static record NegateSampler(DensitySampler input) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, input, outputBuffer, volume, v -> -v);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return -input.sampleValue(context, blockX, blockY, blockZ);
        }
    }
    public static record CubeSampler(DensitySampler input) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, input, outputBuffer, volume, Mth::cube);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return Mth.cube(input.sampleValue(context, blockX, blockY, blockZ));
        }
    }
    public static record LeakyReLUSampler(DensitySampler input, float negativeFactor) implements DensitySampler {
        public static float apply(float factor, float value) {
            return value > 0.0F ? value : value * factor;
        }
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, input, outputBuffer, volume, v -> apply(negativeFactor, v));
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return apply(negativeFactor, input.sampleValue(context, blockX, blockY, blockZ));
        }
    }
    public static record SqueezeSampler(DensitySampler input) implements DensitySampler {
        public static float apply(float value) {
            float c = Mth.clamp(value, -1.0F, 1.0F);
            return c / 2.0F - c * c * c / 24.0F;
        }
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, input, outputBuffer, volume, SqueezeSampler::apply);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return apply(input.sampleValue(context, blockX, blockY, blockZ));
        }
    }
    public static record LogSampler(DensitySampler input) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, input, outputBuffer, volume, v -> (float) Math.log(v));
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return (float) Math.log(input.sampleValue(context, blockX, blockY, blockZ));
        }
    }
    public static record SignSampler(DensitySampler input) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, input, outputBuffer, volume, Math::signum);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return Math.signum(input.sampleValue(context, blockX, blockY, blockZ));
        }
    }
}
