// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.condition;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.PositionalRandomFactory;
import com.mojang.serialization.MapCodec;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.levelgen.VerticalAnchor;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
public record VerticalGradientCondition(Identifier randomName, VerticalAnchor trueAtAndBelow, VerticalAnchor falseAtAndAbove) implements MaterialCondition {
    public static final MapCodec<VerticalGradientCondition> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.material.condition.VerticalGradientCondition.class, "CODEC");
    public MapCodec<VerticalGradientCondition> codec() {
        return CODEC;
    }
    public ConditionEvaluator compile(MaterialRuleContext ruleContext) {
        int below = ruleContext.resolveAnchorY(trueAtAndBelow);
        int above = ruleContext.resolveAnchorY(falseAtAndAbove);
        PositionalRandomFactory factory = ruleContext.getOrCreateRandomFactory(randomName);
        return ruleContext.lazyY(() -> {
            int y = ruleContext.blockY();
            if (y <= below) {
                return true;
            }
            if (y >= above) {
                return false;
            }
            RandomSource random = factory.at(ruleContext.blockX(), y, ruleContext.blockZ());
            return random.nextFloat() < Mth.map((double) y, below, above, 1.0, 0.0);
        });
    }
}
