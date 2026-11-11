// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
public class BlockMatchTest extends RuleTest {
    private final Block block;
    public static final MapCodec<BlockMatchTest> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.BlockMatchTest.class, "CODEC");
    public BlockMatchTest(Block block) {
        this.block = block;
    }
    public boolean test(BlockState blockState, BlockPos pos, RandomSource random) {
        return blockState.is(block);
    }
    protected RuleTestType<?> getType() {
        return RuleTestType.BLOCK_TEST;
    }
}
