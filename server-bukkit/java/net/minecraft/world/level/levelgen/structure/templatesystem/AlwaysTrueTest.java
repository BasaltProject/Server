// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;
public class AlwaysTrueTest extends RuleTest {
    public static final MapCodec<AlwaysTrueTest> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.AlwaysTrueTest.class, "CODEC");
    public static final AlwaysTrueTest INSTANCE = new AlwaysTrueTest();
    private AlwaysTrueTest() {
        
    }
    public boolean testAgainstWorldState(LevelReader level, BlockPos pos, RandomSource random) {
        return true;
    }
    public boolean test(BlockState blockState, BlockPos pos, RandomSource random) {
        return true;
    }
    protected RuleTestType<?> getType() {
        return RuleTestType.ALWAYS_TRUE_TEST;
    }
}
