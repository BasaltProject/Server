// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
public class AxisAlignedLinearPosTest extends PosRuleTest {
    private final float minChance;
    private final float maxChance;
    private final int minDist;
    private final int maxDist;
    private final Direction.Axis axis;
    public static final MapCodec<AxisAlignedLinearPosTest> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.AxisAlignedLinearPosTest.class, "CODEC");
    public AxisAlignedLinearPosTest(float minChance, float maxChance, int minDist, int maxDist, Direction.Axis axis) {
        if (minDist >= maxDist) {
            throw new IllegalArgumentException("Invalid range: [" + minDist + "," + maxDist + "]");
        }
        this.minChance = minChance;
        this.maxChance = maxChance;
        this.minDist = minDist;
        this.maxDist = maxDist;
        this.axis = axis;
    }
    public boolean test(BlockPos inTemplatePos, BlockPos worldPos, BlockPos worldReference, RandomSource random) {
        Direction step = Direction.get(Direction.AxisDirection.POSITIVE, axis);
        float xd = Math.abs((worldPos.getX() - worldReference.getX()) * step.getStepX());
        float yd = Math.abs((worldPos.getY() - worldReference.getY()) * step.getStepY());
        float zd = Math.abs((worldPos.getZ() - worldReference.getZ()) * step.getStepZ());
        int dist = (int) (xd + yd + zd);
        return random.nextFloat() <= net.minecraft.util.Mth.clampedLerp(net.minecraft.util.Mth.inverseLerp((float) dist, (float) minDist, (float) maxDist), minChance, maxChance);
    }
    protected PosRuleTestType<?> getType() {
        return PosRuleTestType.AXIS_ALIGNED_LINEAR_POS_TEST;


    }
}
