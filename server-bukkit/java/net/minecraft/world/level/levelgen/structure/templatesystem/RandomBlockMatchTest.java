// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
public class RandomBlockMatchTest extends RuleTest {
    private final Block block;
    private final float probability;
    public static final MapCodec<RandomBlockMatchTest> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.RandomBlockMatchTest.class, "CODEC");
    public RandomBlockMatchTest(Block block, float probability) {
        this.block = block;
        this.probability = probability;
    }
    public boolean test(BlockState blockState, BlockPos pos, RandomSource random) {
        return blockState.is(block) && random.nextFloat() < probability;
    }
    protected RuleTestType<?> getType() {
        return RuleTestType.RANDOM_BLOCK_TEST;
    }
}
