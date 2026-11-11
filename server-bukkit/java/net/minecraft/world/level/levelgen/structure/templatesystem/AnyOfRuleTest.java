// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.state.BlockState;
public class AnyOfRuleTest extends RuleTest {
    private final List<RuleTest> rules;
    public static final MapCodec<AnyOfRuleTest> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.AnyOfRuleTest.class, "CODEC");
    public AnyOfRuleTest(List<RuleTest> rules) {
        this.rules = rules;
    }
    public boolean test(BlockState blockState, BlockPos pos, RandomSource random) {
        for (RuleTest rule : rules) {
            if (rule.test(blockState, pos, random)) {
                return true;
            }
        }
        return false;
    }
    protected RuleTestType<?> getType() {
        return RuleTestType.ANY_OF_TEST;
    }
}
