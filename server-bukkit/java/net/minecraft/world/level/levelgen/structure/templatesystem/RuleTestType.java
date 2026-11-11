// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
public interface RuleTestType<P extends RuleTest> {
    RuleTestType<AllOfRuleTest> ALL_OF_TEST = of("all_of", AllOfRuleTest.CODEC);
    RuleTestType<AlwaysTrueTest> ALWAYS_TRUE_TEST = of("always_true", AlwaysTrueTest.CODEC);
    RuleTestType<AnyOfRuleTest> ANY_OF_TEST = of("any_of", AnyOfRuleTest.CODEC);
    RuleTestType<BlockMatchTest> BLOCK_TEST = of("block_match", BlockMatchTest.CODEC);
    RuleTestType<BlockStateMatchTest> BLOCKSTATE_TEST = of("blockstate_match", BlockStateMatchTest.CODEC);
    RuleTestType<NotRuleTest> NOT_TEST = of("not", NotRuleTest.CODEC);
    RuleTestType<TagMatchTest> TAG_TEST = of("tag_match", TagMatchTest.CODEC);
    RuleTestType<HeightMatchTest> HEIGHT_TEST = of("height_match", HeightMatchTest.CODEC);
    RuleTestType<RandomBlockMatchTest> RANDOM_BLOCK_TEST = of("random_block_match", RandomBlockMatchTest.CODEC);
    RuleTestType<RandomBlockStateMatchTest> RANDOM_BLOCKSTATE_TEST = of("random_blockstate_match", RandomBlockStateMatchTest.CODEC);
    MapCodec<P> codec();
    private static <P extends RuleTest> RuleTestType<P> of(String id, MapCodec<P> codec) {
        return register(id, codec);
    }
    static <P extends RuleTest> RuleTestType<P> register(String id, MapCodec<P> codec) {
        return () -> codec;
    }
}
