// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.ChestBlockEntity;
import net.minecraft.world.level.block.entity.EnderChestBlockEntity;

public class TrialSpawnerBlock extends BaseEntityBlock {
    public static final net.minecraft.world.level.block.state.properties.BooleanProperty OMINOUS = net.minecraft.world.level.block.state.properties.BlockStateProperties.OMINOUS;
    protected TrialSpawnerBlock(int index) {
        super(index);
    }
}
