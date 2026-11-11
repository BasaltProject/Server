// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
public class PosAlwaysTrueTest extends PosRuleTest {
    public static final MapCodec<PosAlwaysTrueTest> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.PosAlwaysTrueTest.class, "CODEC");
    public static final PosAlwaysTrueTest INSTANCE = new PosAlwaysTrueTest();
    private PosAlwaysTrueTest() {
        
    }
    public boolean test(BlockPos inTemplatePos, BlockPos worldPos, BlockPos worldReference, RandomSource random) {
        return true;
    }
    protected PosRuleTestType<?> getType() {
        return PosRuleTestType.ALWAYS_TRUE_TEST;
    }
}
