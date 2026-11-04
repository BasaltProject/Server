// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.ChestBlockEntity;
import net.minecraft.world.level.block.entity.EnderChestBlockEntity;

public class BeehiveBlock extends BaseEntityBlock {
    public static final net.minecraft.world.level.block.state.properties.IntegerProperty HONEY_LEVEL = net.minecraft.world.level.block.state.properties.BlockStateProperties.HONEY_LEVEL;
    public static final int MAX_HONEY_LEVELS = 5;
    protected BeehiveBlock(int index) {
        super(index);
    }
}
