// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.op;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.world.level.levelgen.densityfunction.JsonDensityFunction;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
public record CacheFunction(DensityFunction input) implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:cache\",\"input\":" + JsonDensityFunction.of(input) + "}";
    }
    public static final MapCodec<CacheFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.op.CacheFunction.class, "CODEC");
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        throw new IllegalStateException("A cache needs deduplicating before it compiles");
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        DensityFunction rewritten = rule.rewrite(input);
        return rewritten == input ? this : new CacheFunction(rewritten);
    }
    public Interval range() {
        return input.range();
    }
    public int domainAxes() {
        return input.domainAxes();
    }
    public MapCodec<CacheFunction> codec() {
        return CODEC;
    }
}
