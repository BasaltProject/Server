// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.state.BlockState;
public class NotRuleTest extends RuleTest {
    private final RuleTest rule;
    public static final MapCodec<NotRuleTest> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.NotRuleTest.class, "CODEC");
    public NotRuleTest(RuleTest rule) {
        this.rule = rule;
    }
    public boolean test(BlockState blockState, BlockPos pos, RandomSource random) {
        return !rule.test(blockState, pos, random);
    }
    protected RuleTestType<?> getType() {
        return RuleTestType.NOT_TEST;
    }
}
