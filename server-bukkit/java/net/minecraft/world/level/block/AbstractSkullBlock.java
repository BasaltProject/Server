// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.block.state.properties.BooleanProperty;

public abstract class AbstractSkullBlock extends BaseEntityBlock {
    public static final BooleanProperty POWERED = BlockStateProperties.POWERED;

    protected AbstractSkullBlock(int index) {
        super(index);
    }

    public SkullBlock.Type getType() {
        String path = blockKey().getPath();
        for (SkullBlock.Types type : SkullBlock.Types.values()) {
            if (path.startsWith(type.getSerializedName() + "_") || path.equals(type.getSerializedName())) {
                return type;
            }
        }
        return SkullBlock.Types.SKELETON;
    }

    protected boolean isPathfindable(BlockState state, net.minecraft.world.level.pathfinder.PathComputationType type) {
        return false;
    }
}
