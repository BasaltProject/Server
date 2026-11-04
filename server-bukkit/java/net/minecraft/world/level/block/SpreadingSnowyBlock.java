// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
public abstract class SpreadingSnowyBlock extends SnowyBlock {
    protected SpreadingSnowyBlock(int index) {
        super(index);
    }
    protected SpreadingSnowyBlock(BlockBehaviour.Properties properties, ResourceKey<Block> baseBlock) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    protected void randomTick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.randomTick(state, level, pos, random);
    }
}
