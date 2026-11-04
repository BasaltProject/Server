// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.ChestBlockEntity;
import net.minecraft.world.level.block.entity.EnderChestBlockEntity;

public class SculkSensorBlock extends BaseEntityBlock implements SimpleWaterloggedBlock {
    protected SculkSensorBlock(int index) {
        super(index);
    }
}
