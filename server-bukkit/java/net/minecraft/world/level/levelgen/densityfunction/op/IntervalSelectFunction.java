// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.op;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.world.level.levelgen.densityfunction.JsonDensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.SamplerKit;
import net.minecraft.world.level.levelgen.densityfunction.ScopedDensityBuffer;
import com.mojang.serialization.MapCodec;
import it.unimi.dsi.fastutil.floats.FloatList;
import java.util.List;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
public record IntervalSelectFunction(DensityFunction input, FloatList thresholds, List<DensityFunction> functions) implements DensityFunction, TreeSource {
    public String treeJson() {
        StringBuilder out = new StringBuilder("{\"type\":\"minecraft:interval_select\",\"input\":").append(JsonDensityFunction.of(input)).append(",\"thresholds\":[");
        for (int i = 0; i < thresholds.size(); i++) {
            out.append(i == 0 ? "" : ",").append(SamplerKit.number(thresholds.getFloat(i)));
        }
        out.append("],\"functions\":[");
        for (int i = 0; i < functions.size(); i++) {
            out.append(i == 0 ? "" : ",").append(JsonDensityFunction.of(functions.get(i)));
        }
        return out.append("]}").toString();
    }
    public static final MapCodec<IntervalSelectFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.op.IntervalSelectFunction.class, "CODEC");
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        DensitySampler in = input.compileSampler(context);
        if (thresholds.size() == 1) {
            return new SingleThresholdSampler(in, thresholds.getFloat(0), functions.get(0).compileSampler(context), functions.get(1).compileSampler(context));
        }
        DensitySampler[] compiled = new DensitySampler[functions.size()];
        for (int i = 0; i < compiled.length; i++) {
            compiled[i] = functions.get(i).compileSampler(context);
        }
        return new Sampler(in, thresholds.toFloatArray(), compiled);
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        DensityFunction in = rule.rewrite(input);
        boolean changed = in != input;
        List<DensityFunction> mapped = new java.util.ArrayList<>(functions.size());
        for (DensityFunction function : functions) {
            DensityFunction rewritten = rule.rewrite(function);
            changed |= rewritten != function;
            mapped.add(rewritten);
        }
        return changed ? new IntervalSelectFunction(in, thresholds, mapped) : this;
    }
    public Interval range() {
        return Interval.encapsulating(functions.stream().map(DensityFunction::range).toList());
    }
    public int domainAxes() {
        int axes = input.domainAxes();
        for (DensityFunction function : functions) {
            axes |= function.domainAxes();
        }
        return axes;
    }
    public MapCodec<IntervalSelectFunction> codec() {
        return CODEC;
    }
    static record Sampler(DensitySampler input, float[] thresholds, DensitySampler[] samplers) implements DensitySampler {
        private int pick(float value) {
            for (int i = 0; i < thresholds.length; i++) {
                if (value < thresholds[i]) {
                    return i;
                }
            }
            return samplers.length - 1;
        }
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        input.sampleVolume(context, outputBuffer, volume);
        ScopedDensityBuffer[] columns = new ScopedDensityBuffer[samplers.length];
        try {
            for (int i = 0; i < columns.length; i++) {
                columns[i] = context.acquireBuffer(volume);
                samplers[i].sampleVolume(context, columns[i], volume);
            }
            for (int i = 0; i < outputBuffer.size(); i++) {
                outputBuffer.set(i, columns[pick(outputBuffer.get(i))].get(i));
            }
        } finally {
            for (ScopedDensityBuffer column : columns) {
                if (column != null) {
                    column.close();
                }
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return samplers[pick(input.sampleValue(context, blockX, blockY, blockZ))].sampleValue(context, blockX, blockY, blockZ);
        }
    }
    static record SingleThresholdSampler(DensitySampler input, float threshold, DensitySampler ifBelow, DensitySampler ifAbove) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        input.sampleVolume(context, outputBuffer, volume);
        try (ScopedDensityBuffer below = context.acquireBuffer(volume); ScopedDensityBuffer above = context.acquireBuffer(volume)) {
            ifBelow.sampleVolume(context, below, volume);
            ifAbove.sampleVolume(context, above, volume);
            for (int i = 0; i < outputBuffer.size(); i++) {
                outputBuffer.set(i, outputBuffer.get(i) < threshold ? below.get(i) : above.get(i));
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return input.sampleValue(context, blockX, blockY, blockZ) < threshold
            ? ifBelow.sampleValue(context, blockX, blockY, blockZ)
            : ifAbove.sampleValue(context, blockX, blockY, blockZ);
        }
    }
}
