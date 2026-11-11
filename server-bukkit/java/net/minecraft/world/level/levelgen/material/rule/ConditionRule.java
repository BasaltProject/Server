// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.rule;
import net.minecraft.world.level.levelgen.material.condition.ConditionEvaluator;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
import net.minecraft.world.level.levelgen.material.condition.MaterialCondition;
public record ConditionRule(MaterialCondition ifTrue, MaterialRule thenRun) implements MaterialRule {
    public static final MapCodec<ConditionRule> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.material.rule.ConditionRule.class, "CODEC");
    public MapCodec<ConditionRule> codec() {
        return CODEC;
    }
    public RuleEvaluator compile(MaterialRuleContext context) {
        ConditionEvaluator gate = ifTrue.compile(context);
        RuleEvaluator body = thenRun.compile(context);
        return (x, y, z) -> gate.test() ? body.tryApply(x, y, z) : null;
    }
}
