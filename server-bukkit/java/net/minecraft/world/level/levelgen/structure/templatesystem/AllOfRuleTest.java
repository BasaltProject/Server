// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.state.BlockState;
public class AllOfRuleTest extends RuleTest {
    private final List<RuleTest> rules;
    public static final MapCodec<AllOfRuleTest> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.AllOfRuleTest.class, "CODEC");
    public AllOfRuleTest(List<RuleTest> rules) {
        this.rules = rules;
    }
    public boolean test(BlockState blockState, BlockPos pos, RandomSource random) {
        for (RuleTest rule : rules) {
            if (!rule.test(blockState, pos, random)) {
                return false;
            }
        }
        return true;
    }
    protected RuleTestType<?> getType() {
        return RuleTestType.ALL_OF_TEST;
    }
}
