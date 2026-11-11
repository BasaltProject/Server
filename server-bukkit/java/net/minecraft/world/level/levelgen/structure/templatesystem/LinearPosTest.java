// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
public class LinearPosTest extends PosRuleTest {
    private final float minChance;
    private final float maxChance;
    private final int minDist;
    private final int maxDist;
    public static final MapCodec<LinearPosTest> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.LinearPosTest.class, "CODEC");
    public LinearPosTest(float minChance, float maxChance, int minDist, int maxDist) {
        if (minDist >= maxDist) {
            throw new IllegalArgumentException("Invalid range: [" + minDist + "," + maxDist + "]");
        }
        this.minChance = minChance;
        this.maxChance = maxChance;
        this.minDist = minDist;
        this.maxDist = maxDist;
    }
    public boolean test(BlockPos inTemplatePos, BlockPos worldPos, BlockPos worldReference, RandomSource random) {
        int dist = worldPos.distManhattan(worldReference);
        return random.nextFloat() <= net.minecraft.util.Mth.clampedLerp(net.minecraft.util.Mth.inverseLerp((float) dist, (float) minDist, (float) maxDist), minChance, maxChance);
    }
    protected PosRuleTestType<?> getType() {
        return PosRuleTestType.LINEAR_POS_TEST;
    }
}
