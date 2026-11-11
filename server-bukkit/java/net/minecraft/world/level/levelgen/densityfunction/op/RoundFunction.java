// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.op;
import net.minecraft.world.level.levelgen.densityfunction.SamplerKit;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.world.level.levelgen.densityfunction.JsonDensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunctions;
import net.minecraft.world.level.levelgen.densityfunction.generator.ConstantFunction;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
public record RoundFunction(Type type, DensityFunction input, DensityFunction multiple) implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:" + type.id + "\",\"input\":" + JsonDensityFunction.of(input) + ",\"multiple\":" + JsonDensityFunction.of(multiple) + "}";
    }
    static float snap(float value, Type type) {
        return switch (type) {
            case FLOOR -> (float) Math.floor(value);
            case ROUND -> (float) Math.round(value);
            case CEIL -> (float) Math.ceil(value);
            case TRUNCATE -> value > 0.0F ? (float) Math.floor(value) : (float) Math.ceil(value);
        };
    }
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        DensitySampler in = input.compileSampler(context);
        Float step = ConstantFunction.valueOf(multiple);
        return step != null && step == 1.0F ? new IntegerMultipleSampler(type, in) : new Sampler(type, in, multiple.compileSampler(context));
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        DensityFunction in = rule.rewrite(input);
        DensityFunction step = rule.rewrite(multiple);
        return in == input && step == multiple ? this : new RoundFunction(type, in, step);
    }
    public Interval range() {
        Interval step = multiple.range();
        return Interval.mul(Interval.mapMonotonic(Interval.div(input.range(), step), v -> snap(v, type)), step);
    }
    public int domainAxes() {
        return input.domainAxes() | multiple.domainAxes();
    }
    public MapCodec<RoundFunction> codec() {
        return type.codec;
    }
    public static enum Type {
        FLOOR("floor"), ROUND("round"), CEIL("ceil"), TRUNCATE("truncate");
        public final String id;
        public final MapCodec<RoundFunction> codec;
        Type(String id) {
            this.id = id;
            this.codec = RecordCodecBuilder.<RoundFunction>mapCodec(i -> i.group(
                DensityFunction.CODEC.fieldOf("input").forGetter(RoundFunction::input),
                DensityFunction.CODEC.optionalFieldOf("multiple", DensityFunctions.constant(1.0F)).forGetter(RoundFunction::multiple))
                .apply(i, (in, step) -> new RoundFunction(this, in, step)));
        }

    }
    static record Sampler(Type type, DensitySampler input, DensitySampler multiple) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.zip(context, input, multiple, outputBuffer, volume, (v, step) -> step == 0.0F ? v : snap(v / step, type) * step);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        float v = input.sampleValue(context, blockX, blockY, blockZ);
        float step = multiple.sampleValue(context, blockX, blockY, blockZ);
        return step == 0.0F ? v : snap(v / step, type) * step;
        }
    }
    static record IntegerMultipleSampler(Type type, DensitySampler input) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, input, outputBuffer, volume, v -> snap(v, type));
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return snap(input.sampleValue(context, blockX, blockY, blockZ), type);
        }
    }
}
