// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.ChestBlockEntity;
import net.minecraft.world.level.block.entity.EnderChestBlockEntity;

public class CreakingHeartBlock extends BaseEntityBlock {
    public static final net.minecraft.world.level.block.state.properties.BooleanProperty NATURAL = net.minecraft.world.level.block.state.properties.BlockStateProperties.NATURAL;
    protected CreakingHeartBlock(int index) {
        super(index);
    }
}
