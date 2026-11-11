// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.tags.TagKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
public class TagMatchTest extends RuleTest {
    private final TagKey<Block> tag;
    public static final MapCodec<TagMatchTest> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.TagMatchTest.class, "CODEC");
    public TagMatchTest(TagKey<Block> tag) {
        this.tag = tag;
    }
    public boolean test(BlockState blockState, BlockPos pos, RandomSource random) {
        return blockState.is(tag);
    }
    protected RuleTestType<?> getType() {
        return RuleTestType.TAG_TEST;
    }
}
