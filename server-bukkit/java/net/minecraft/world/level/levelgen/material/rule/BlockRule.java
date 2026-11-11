// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.rule;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
public record BlockRule(BlockState resultState) implements MaterialRule, RuleEvaluator {
    public static final MapCodec<BlockRule> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.material.rule.BlockRule.class, "CODEC");
    public MapCodec<BlockRule> codec() {
        return CODEC;
    }
    public RuleEvaluator compile(MaterialRuleContext context) {
        return this;
    }
    public BlockState tryApply(int blockX, int blockY, int blockZ) {
        return resultState;
    }
}
