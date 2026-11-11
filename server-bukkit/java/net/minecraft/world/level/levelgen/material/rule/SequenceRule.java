// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.rule;
import net.minecraft.world.level.block.state.BlockState;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
public record SequenceRule(List<MaterialRule> sequence) implements MaterialRule {
    public static final MapCodec<SequenceRule> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.material.rule.SequenceRule.class, "CODEC");
    public MapCodec<SequenceRule> codec() {
        return CODEC;
    }
    public RuleEvaluator compile(MaterialRuleContext context) {
        RuleEvaluator[] steps = new RuleEvaluator[sequence.size()];
        for (int i = 0; i < steps.length; i++) {
            steps[i] = sequence.get(i).compile(context);
        }
        if (steps.length == 1) {
            return steps[0];
        }
        return (x, y, z) -> {
            for (RuleEvaluator step : steps) {
                BlockState state = step.tryApply(x, y, z);
                if (state != null) {
                    return state;
                }
            }
            return null;
        };
    }
}
