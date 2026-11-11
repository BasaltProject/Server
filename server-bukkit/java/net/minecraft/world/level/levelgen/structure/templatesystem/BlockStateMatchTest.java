// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.state.BlockState;
public class BlockStateMatchTest extends RuleTest {
    private final BlockState state;
    public static final MapCodec<BlockStateMatchTest> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.BlockStateMatchTest.class, "CODEC");
    public BlockStateMatchTest(BlockState blockState) {
        this.state = blockState;
    }
    public boolean test(BlockState blockState, BlockPos pos, RandomSource random) {
        return blockState == state;
    }
    protected RuleTestType<?> getType() {
        return RuleTestType.BLOCKSTATE_TEST;
    }
}
