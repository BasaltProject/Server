// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.util.ColorRGBA;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
public class ColoredFallingBlock extends FallingBlock {
    protected ColoredFallingBlock(int index) {
        super(index);
    }
    protected final ColorRGBA dustColor = null;
    public ColoredFallingBlock(ColorRGBA dustColor, BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    public int getDustColor(BlockState blockState, BlockGetter level, BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.ColoredFallingBlock#getDustColor");
    }
}
