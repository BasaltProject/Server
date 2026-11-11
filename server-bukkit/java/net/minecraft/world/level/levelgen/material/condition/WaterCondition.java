// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.condition;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
public record WaterCondition(int offset, int surfaceDepthMultiplier, boolean addStoneDepth) implements MaterialCondition {
    public static final MapCodec<WaterCondition> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.material.condition.WaterCondition.class, "CODEC");
    public MapCodec<WaterCondition> codec() {
        return CODEC;
    }
    public ConditionEvaluator compile(MaterialRuleContext ruleContext) {
        return ruleContext.lazyY(() -> {
            int water = ruleContext.waterHeight();
            int depth = addStoneDepth ? ruleContext.stoneDepthAbove() : 0;
            return water == Integer.MIN_VALUE
                || ruleContext.blockY() + depth >= water + offset + ruleContext.surfaceDepth() * surfaceDepthMultiplier;
        });
    }
}
