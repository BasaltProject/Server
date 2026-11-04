// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
public class IceBlock extends HalfTransparentBlock {
    protected IceBlock(int index) {
        super(index);
    }
    public IceBlock(BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    public static BlockState meltsInto() {
        return Blocks.WATER.defaultBlockState();
    }
    public void playerDestroy(ServerLevel level, ServerPlayer player, BlockPos pos, BlockState state, BlockEntity blockEntity, ItemStack destroyedWith) {
        super.playerDestroy(level, player, pos, state, blockEntity, destroyedWith);
    }
    protected void randomTick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.randomTick(state, level, pos, random);
    }
    protected void melt(BlockState state, Level level, BlockPos pos) {
        boolean ultrawarm = level instanceof net.minecraft.server.level.ServerLevel server
            && server.getWorld().getEnvironment() == org.bukkit.World.Environment.NETHER;
        level.setBlock(pos, ultrawarm ? Blocks.AIR.defaultBlockState() : Blocks.WATER.defaultBlockState(), 3);
    }
}
