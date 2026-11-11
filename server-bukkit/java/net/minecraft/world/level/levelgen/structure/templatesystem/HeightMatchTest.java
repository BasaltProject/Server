// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.state.BlockState;
public class HeightMatchTest extends RuleTest {
    private final int minInclusive;
    private final int maxInclusive;
    public static final MapCodec<HeightMatchTest> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.HeightMatchTest.class, "CODEC");
    public HeightMatchTest(int minInclusive, int maxInclusive) {
        this.minInclusive = minInclusive;
        this.maxInclusive = maxInclusive;
    }
    public static RuleTest min(int minInclusive) {
        return new HeightMatchTest(minInclusive, net.minecraft.world.level.dimension.DimensionType.MAX_Y);
    }
    public static RuleTest max(int maxInclusive) {
        return new HeightMatchTest(net.minecraft.world.level.dimension.DimensionType.MIN_Y, maxInclusive);
    }
    public boolean test(BlockState blockState, BlockPos pos, RandomSource random) {
        return minInclusive <= pos.getY() && pos.getY() <= maxInclusive;
    }
    protected RuleTestType<?> getType() {
        return RuleTestType.HEIGHT_TEST;
    }
}
