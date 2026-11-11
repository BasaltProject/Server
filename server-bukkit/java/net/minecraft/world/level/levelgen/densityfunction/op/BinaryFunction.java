// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.op;
import net.minecraft.world.level.levelgen.densityfunction.SamplerKit;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.world.level.levelgen.densityfunction.JsonDensityFunction;
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
public record BinaryFunction(Type type, DensityFunction left, DensityFunction right) implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:" + type.id + "\",\"left\":" + JsonDensityFunction.of(left) + ",\"right\":" + JsonDensityFunction.of(right) + "}";
    }
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        DensitySampler l = left.compileSampler(context);
        DensitySampler r = right.compileSampler(context);
        Float lc = ConstantFunction.valueOf(left);
        Float rc = ConstantFunction.valueOf(right);
        switch (type) {
            case ADD:
                return lc != null ? new ConstAddSampler(r, lc) : rc != null ? new ConstAddSampler(l, rc) : new AddSampler(l, r);
            case SUB:
                return lc != null ? new ConstSubSampler(lc, r) : rc != null ? new ConstAddSampler(l, -rc) : new SubSampler(l, r);
            case MUL:
                return lc != null ? new ConstMulSampler(r, lc) : rc != null ? new ConstMulSampler(l, rc) : new MulSampler(l, r);
            case DIV:
                return lc != null ? new ConstDivSampler(lc, r) : rc != null ? new ConstMulSampler(l, 1.0F / rc) : new DivSampler(l, r);
            case MIN: {
                Interval lr = left.range();
                Interval rr = right.range();
                if (lr.max() < rr.min()) {
                    return l;
                }
                if (rr.max() < lr.min()) {
                    return r;
                }
                return lc != null ? new ConstMinSampler(r, lc) : rc != null ? new ConstMinSampler(l, rc) : new MinSampler(l, r, rr.min());
            }
            default: {
                Interval lr = left.range();
                Interval rr = right.range();
                if (lr.min() > rr.max()) {
                    return l;
                }
                if (rr.min() > lr.max()) {
                    return r;
                }
                return lc != null ? new ConstMaxSampler(r, lc) : rc != null ? new ConstMaxSampler(l, rc) : new MaxSampler(l, r, rr.max());
            }
        }
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        DensityFunction a = rule.rewrite(left);
        DensityFunction b = rule.rewrite(right);
        return a == left && b == right ? this : new BinaryFunction(type, a, b);
    }
    public Interval range() {
        Interval a = left.range();
        Interval b = right.range();
        return switch (type) {
            case ADD -> Interval.add(a, b);
            case SUB -> Interval.sub(a, b);
            case MUL -> Interval.mul(a, b);
            case DIV -> Interval.div(a, b);
            case MIN -> Interval.min(a, b);
            case MAX -> Interval.max(a, b);
        };
    }
    public int domainAxes() {
        return left.domainAxes() | right.domainAxes();
    }
    public MapCodec<BinaryFunction> codec() {
        return type.codec;
    }
    public static enum Type {
        ADD("add"), SUB("sub"), MUL("mul"), DIV("div"), MIN("min"), MAX("max");
        public final String id;
        public final MapCodec<BinaryFunction> codec;
        Type(String id) {
            this.id = id;
            this.codec = RecordCodecBuilder.<BinaryFunction>mapCodec(i -> i.group(
                DensityFunction.CODEC.fieldOf("left").forGetter(BinaryFunction::left),
                DensityFunction.CODEC.fieldOf("right").forGetter(BinaryFunction::right)).apply(i, (l, r) -> new BinaryFunction(this, l, r)));
        }

    }
    public static record AddSampler(DensitySampler left, DensitySampler right) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.zip(context, left, right, outputBuffer, volume, (a, b) -> a + b);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return left.sampleValue(context, blockX, blockY, blockZ) + right.sampleValue(context, blockX, blockY, blockZ);
        }
    }
    public static record ConstAddSampler(DensitySampler left, float right) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, left, outputBuffer, volume, v -> v + right);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return left.sampleValue(context, blockX, blockY, blockZ) + right;
        }
    }
    public static record MulSampler(DensitySampler left, DensitySampler right) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.zip(context, left, right, outputBuffer, volume, (a, b) -> a * b);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        float first = left.sampleValue(context, blockX, blockY, blockZ);
        return first == 0.0F ? 0.0F : first * right.sampleValue(context, blockX, blockY, blockZ);
        }
    }
    public static record ConstMulSampler(DensitySampler left, float right) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, left, outputBuffer, volume, v -> v * right);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return left.sampleValue(context, blockX, blockY, blockZ) * right;
        }
    }
    public static record ConstDivSampler(float left, DensitySampler right) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, right, outputBuffer, volume, v -> left / v);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return left / right.sampleValue(context, blockX, blockY, blockZ);
        }
    }
    public static record SubSampler(DensitySampler left, DensitySampler right) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.zip(context, left, right, outputBuffer, volume, (a, b) -> a - b);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return left.sampleValue(context, blockX, blockY, blockZ) - right.sampleValue(context, blockX, blockY, blockZ);
        }
    }
    public static record ConstSubSampler(float left, DensitySampler right) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, right, outputBuffer, volume, v -> left - v);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return left - right.sampleValue(context, blockX, blockY, blockZ);
        }
    }
    public static record DivSampler(DensitySampler left, DensitySampler right) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.zip(context, left, right, outputBuffer, volume, (a, b) -> a / b);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        float first = left.sampleValue(context, blockX, blockY, blockZ);
        return first == 0.0F ? 0.0F : first / right.sampleValue(context, blockX, blockY, blockZ);
        }
    }
    public static record MinSampler(DensitySampler left, DensitySampler right, float rightMinValue) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.zip(context, left, right, outputBuffer, volume, (a, b) -> b < a ? b : a);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        float first = left.sampleValue(context, blockX, blockY, blockZ);
        return first <= rightMinValue ? first : Math.min(first, right.sampleValue(context, blockX, blockY, blockZ));
        }
    }
    public static record MaxSampler(DensitySampler left, DensitySampler right, float rightMaxValue) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.zip(context, left, right, outputBuffer, volume, (a, b) -> b > a ? b : a);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        float first = left.sampleValue(context, blockX, blockY, blockZ);
        return first >= rightMaxValue ? first : Math.max(first, right.sampleValue(context, blockX, blockY, blockZ));
        }
    }
    public static record ConstMinSampler(DensitySampler left, float right) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, left, outputBuffer, volume, v -> right < v ? right : v);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return Math.min(left.sampleValue(context, blockX, blockY, blockZ), right);
        }
    }
    public static record ConstMaxSampler(DensitySampler left, float right) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        SamplerKit.map(context, left, outputBuffer, volume, v -> right > v ? right : v);
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return Math.max(left.sampleValue(context, blockX, blockY, blockZ), right);

        }
    }
}
