// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.condition;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.levelgen.VerticalAnchor;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
public record YCondition(VerticalAnchor anchor, int surfaceDepthMultiplier, boolean addStoneDepth) implements MaterialCondition {
    public static final MapCodec<YCondition> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.material.condition.YCondition.class, "CODEC");
    public MapCodec<YCondition> codec() {
        return CODEC;
    }
    public ConditionEvaluator compile(MaterialRuleContext ruleContext) {
        return ruleContext.lazyY(() -> {
            int depth = addStoneDepth ? ruleContext.stoneDepthAbove() : 0;
            return ruleContext.blockY() + depth
                >= ruleContext.resolveAnchorY(anchor) + ruleContext.surfaceDepth() * surfaceDepthMultiplier;
        });
    }
}
