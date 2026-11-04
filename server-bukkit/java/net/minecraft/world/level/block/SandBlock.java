// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.util.ColorRGBA;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
public class SandBlock extends ColoredFallingBlock {
    protected SandBlock(int index) {
        super(index);
    }
    public SandBlock(ColorRGBA dustColor, BlockBehaviour.Properties properties) {
        super((net.minecraft.util.ColorRGBA) null, (net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    public void animateTick(BlockState state, Level level, BlockPos pos, RandomSource random) {
        super.animateTick(state, level, pos, random);
    }
}
