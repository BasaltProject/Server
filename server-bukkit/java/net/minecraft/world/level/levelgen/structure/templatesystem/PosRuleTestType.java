// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
public interface PosRuleTestType<P extends PosRuleTest> {
    PosRuleTestType<PosAlwaysTrueTest> ALWAYS_TRUE_TEST = of("always_true", PosAlwaysTrueTest.CODEC);
    PosRuleTestType<LinearPosTest> LINEAR_POS_TEST = of("linear_pos", LinearPosTest.CODEC);
    PosRuleTestType<AxisAlignedLinearPosTest> AXIS_ALIGNED_LINEAR_POS_TEST = of("axis_aligned_linear_pos", AxisAlignedLinearPosTest.CODEC);
    MapCodec<P> codec();
    private static <P extends PosRuleTest> PosRuleTestType<P> of(String id, MapCodec<P> codec) {
        return register(id, codec);
    }
    static <P extends PosRuleTest> PosRuleTestType<P> register(String id, MapCodec<P> codec) {
        return () -> codec;
    }
}
