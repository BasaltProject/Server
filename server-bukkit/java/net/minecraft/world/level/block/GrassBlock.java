// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
public class GrassBlock extends SpreadingSnowyBlock implements BonemealableBlock {
    protected GrassBlock(int index) {
        super(index);
    }
    public GrassBlock(BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null, (net.minecraft.resources.ResourceKey) null);
    }
    public boolean isValidBonemealTarget(LevelReader level, BlockPos pos, BlockState state, BonemealSource source) {
        return level.getBlockState(pos.above()).isAir();
    }
    public boolean isBonemealSuccess(Level level, RandomSource random, BlockPos pos, BlockState state, BonemealSource source) {
        return true;
    }
    public void performBonemeal(ServerLevel level, RandomSource random, BlockPos pos, BlockState state, BonemealSource source) {
        org.basaltmc.bukkit.WorldEnvNatives.boneMeal(level.getWorld().getName(), pos.getX(), pos.getY(), pos.getZ());
    }
    public BonemealableBlock.Type getType() {
        return BonemealableBlock.Type.NEIGHBOR_SPREADER;
    }
}
