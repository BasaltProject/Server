// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.core.Direction;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.properties.EnumProperty;
public abstract class DirectionalBlock extends Block {
    protected DirectionalBlock(int index) {
        super(index);
    }
    public static final EnumProperty<Direction> FACING = BlockStateProperties.FACING;
    protected DirectionalBlock(BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
}
