// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.condition;
import net.minecraft.util.Mth;
import net.minecraft.world.level.levelgen.material.condition.ConditionEvaluator;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
import net.minecraft.world.level.levelgen.placement.CaveSurface;
public record StoneDepthCondition(int offset, boolean addSurfaceDepth, int secondaryDepthRange, CaveSurface surfaceType) implements MaterialCondition {
    public static final MapCodec<StoneDepthCondition> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.material.condition.StoneDepthCondition.class, "CODEC");
    public MapCodec<StoneDepthCondition> codec() {
        return CODEC;
    }
    public ConditionEvaluator compile(MaterialRuleContext ruleContext) {
        boolean ceiling = surfaceType == CaveSurface.CEILING;
        return ruleContext.lazyY(() -> {
            int depth = ceiling ? ruleContext.stoneDepthBelow() : ruleContext.stoneDepthAbove();
            int primary = addSurfaceDepth ? ruleContext.surfaceDepth() : 0;
            int secondary = secondaryDepthRange == 0 ? 0
                : (int) Mth.map(ruleContext.getSurfaceSecondary(), -1.0, 1.0, 0.0, secondaryDepthRange);
            return depth <= 1 + offset + primary + secondary;
        });
    }
}
