// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.generator;
import net.minecraft.world.level.levelgen.densityfunction.ContextBoundSampler;
import net.minecraft.world.level.levelgen.blending.Blender;
import net.minecraft.world.level.levelgen.Beardifier;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
public enum SimpleDensityFunction implements DensityFunction, net.minecraft.world.level.levelgen.densityfunction.TreeSource {
    BLEND_ALPHA("blend_alpha"), BLEND_OFFSET("blend_offset"), BEARDIFIER("beardifier");
    private final String id;
    private final MapCodec<SimpleDensityFunction> codec = MapCodec.unit(this);
    SimpleDensityFunction(String id) {
        this.id = id;
    }
    public String treeJson() {
        return "{\"type\":\"minecraft:" + id + "\"}";
    }
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        return switch (this) {
            case BLEND_ALPHA -> new ContextBoundSampler(Blender.ALPHA_KEY, new ConstantFunction.Sampler(1.0F));
            case BLEND_OFFSET -> new ContextBoundSampler(Blender.OFFSET_KEY, new ConstantFunction.Sampler(0.0F));
            case BEARDIFIER -> new ContextBoundSampler(Beardifier.CONTEXT_KEY, new ConstantFunction.Sampler(0.0F));
        };
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        return this;
    }
    public Interval range() {
        return switch (this) {
            case BLEND_ALPHA -> Interval.of(0.0F, 1.0F);
            case BLEND_OFFSET -> Interval.INFINITE;
            case BEARDIFIER -> Beardifier.RANGE;
        };
    }
    public int domainAxes() {
        return this == BEARDIFIER ? ALL_AXES : AXIS_X | AXIS_Z;
    }
    public String id() {
        return id;
    }
    public MapCodec<SimpleDensityFunction> codec() {
        return codec;

    }
}
