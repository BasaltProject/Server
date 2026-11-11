// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
public class InfestedBlock extends Block {
    private Block hostBlock;
    private static boolean hasInfested(Block host) {
        return host != null && Blocks.named("minecraft:infested_" + host.blockKey().getPath()) != null;
    }
    public InfestedBlock(Block hostBlock, BlockBehaviour.Properties properties) {
        super((int) 0);
        this.hostBlock = hostBlock;
    }
    public Block getHostBlock() {
        return hostBlock;
    }
    public static boolean isCompatibleHostBlock(BlockState blockState) {
        return hasInfested(blockState.getBlock());
    }
    protected void spawnAfterBreak(BlockState state, ServerLevel level, BlockPos pos, ItemStack tool, boolean dropExperience) {
        super.spawnAfterBreak(state, level, pos, tool, dropExperience);
    }
    public static BlockState infestedStateByHost(BlockState hostState) {
        Block infested = Blocks.named("minecraft:infested_" + hostState.getBlock().blockKey().getPath());
        return infested.withPropertiesOf(hostState);
    }
    public BlockState hostStateByInfested(BlockState infestedState) {
        return getHostBlock().withPropertiesOf(infestedState);
    }
}
