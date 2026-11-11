// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.state.BlockState;
public class RandomBlockStateMatchTest extends RuleTest {
    private final BlockState state;
    private final float probability;
    public static final MapCodec<RandomBlockStateMatchTest> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.RandomBlockStateMatchTest.class, "CODEC");
    public RandomBlockStateMatchTest(BlockState blockState, float probability) {
        this.state = blockState;
        this.probability = probability;
    }
    public boolean test(BlockState blockState, BlockPos pos, RandomSource random) {
        return blockState == state && random.nextFloat() < probability;
    }
    protected RuleTestType<?> getType() {
        return RuleTestType.RANDOM_BLOCKSTATE_TEST;
    }
}
