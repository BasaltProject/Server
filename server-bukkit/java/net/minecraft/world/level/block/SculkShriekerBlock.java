// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.ChestBlockEntity;
import net.minecraft.world.level.block.entity.EnderChestBlockEntity;

public class SculkShriekerBlock extends BaseEntityBlock implements SimpleWaterloggedBlock {
    public static final net.minecraft.world.level.block.state.properties.BooleanProperty SHRIEKING = net.minecraft.world.level.block.state.properties.BlockStateProperties.SHRIEKING;
    public static final net.minecraft.world.level.block.state.properties.BooleanProperty CAN_SUMMON = net.minecraft.world.level.block.state.properties.BlockStateProperties.CAN_SUMMON;
    protected SculkShriekerBlock(int index) {
        super(index);
    }
}
